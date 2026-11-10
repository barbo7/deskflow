// SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

// Hands the state of a page to Deskflow when a tab moves to another computer, and puts that state
// back when one arrives. Nothing runs inside a page except at those two moments.

const HOST_NAME = 'com.deskflow.handoff';
const MAX_TABS = 40;
const MAX_STATE_PER_TAB = 150 * 1024;

let port = null;

function browserName() {
  if (navigator.brave) {
    return 'brave';
  }
  const brands = (navigator.userAgentData && navigator.userAgentData.brands) || [];
  return brands.some((b) => b.brand === 'Microsoft Edge') ? 'edge' : 'chrome';
}

function connect() {
  if (port) {
    return;
  }
  try {
    port = chrome.runtime.connectNative(HOST_NAME);
  } catch (e) {
    port = null;
    return;
  }
  port.onMessage.addListener(onHostMessage);
  port.onDisconnect.addListener(() => {
    void chrome.runtime.lastError;
    port = null;
    showConnected(false);
  });
  port.postMessage({ type: 'hello', browser: browserName() });
  // the host says when it reaches deskflow
  showConnected(false);
}

// the toolbar button says whether deskflow can be reached; a disconnect usually means deskflow
// isn't running, and the reconnect alarm picks it up again
function showConnected(connected) {
  chrome.action.setTitle({
    title: connected ? 'Deskflow Handoff: ready' : 'Deskflow Handoff: Deskflow is not running',
  });
  chrome.action.setBadgeText({ text: connected ? '' : '!' });
  chrome.action.setBadgeBackgroundColor({ color: '#d93025' });
}

// a copy loaded unpacked from the deskflow folder reloads itself when its files change, so an
// update to deskflow doesn't also need a trip to chrome://extensions
let loadedFiles = null;

async function readOwnFiles() {
  const names = ['manifest.json', 'background.js'];
  const texts = await Promise.all(
    names.map((name) => fetch(chrome.runtime.getURL(name), { cache: 'no-store' }).then((r) => r.text()))
  );
  return texts.join('\n');
}

async function reloadIfChanged() {
  try {
    if ((await chrome.management.getSelf()).installType !== 'development') {
      return;
    }
    const files = await readOwnFiles();
    if (loadedFiles === null) {
      loadedFiles = files;
    } else if (files !== loadedFiles) {
      chrome.runtime.reload();
    }
  } catch (e) {
    // can't tell; leave things as they are
  }
}

// the open port is what keeps this service worker alive; the alarm brings it back if it ever goes
chrome.runtime.onStartup.addListener(connect);
chrome.runtime.onInstalled.addListener(connect);
chrome.alarms.create('reconnect', { periodInMinutes: 1 });
chrome.alarms.onAlarm.addListener(() => {
  connect();
  reloadIfChanged();
});
connect();
reloadIfChanged();

function onHostMessage(message) {
  if (message.type === 'status') {
    showConnected(Boolean(message.connected));
  } else if (message.type === 'capture') {
    capture(message).catch((e) => reply(message.id, { tabs: [], tabIds: [], error: String((e && e.message) || e) }));
  } else if (message.type === 'close') {
    closeTabs(message.tabIds || [], 0);
  } else if (message.type === 'restore') {
    restore(message).catch(() => {});
  }
}

function reply(id, state) {
  if (port) {
    port.postMessage({ type: 'captured', id, state });
  }
}

function isWebPage(url) {
  return typeof url === 'string' && /^https?:\/\//i.test(url);
}

// chrome refuses to move a tab while the user is dragging one, which is the only way to tell
async function isTabBeingDragged(tab) {
  try {
    await chrome.tabs.move(tab.id, { index: tab.index });
    return false;
  } catch (e) {
    return /drag/i.test(String(e && e.message));
  }
}

// the same page as far as the address bar shows it: windows hides "https://" and "www.", and
// may drop the trailing slash
function looseAddress(url) {
  try {
    const parsed = new URL(url);
    return parsed.hostname.replace(/^www\./, '') + parsed.pathname.replace(/\/$/, '') + parsed.search;
  } catch (e) {
    return null;
  }
}

function hostOf(url) {
  try {
    return new URL(url).hostname.replace(/^www\./, '');
  } catch (e) {
    return null;
  }
}

// the window deskflow saw being dragged shows `hint`; the one the browser last focused may be
// another (say, the extensions page)
async function findSentTab(hint) {
  const [focused] = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
  if (!hint) {
    return focused;
  }
  const actives = await chrome.tabs.query({ active: true });
  // the focused window first, when it's a match
  const ordered = focused ? [focused, ...actives.filter((tab) => tab.id !== focused.id)] : actives;
  const matchers = [
    (tab) => tab.url === hint,
    (tab) => looseAddress(tab.url) !== null && looseAddress(tab.url) === looseAddress(hint),
    (tab) => hostOf(tab.url) !== null && hostOf(tab.url) === hostOf(hint),
  ];
  for (const matches of matchers) {
    const found = ordered.find(matches);
    if (found) {
      return found;
    }
  }
  return focused;
}

async function capture(message) {
  const active = await findSentTab(message.url);
  if (!active) {
    reply(message.id, { tabs: [], tabIds: [], error: 'no active tab' });
    return;
  }

  // a tab held by its tab moves alone; a window held by its title bar moves with all its tabs
  const wholeWindow = message.mode === 'drag' && !(await isTabBeingDragged(active));
  const candidates = wholeWindow ? await chrome.tabs.query({ windowId: active.windowId }) : [active];
  const tabs = candidates.filter((tab) => isWebPage(tab.url)).slice(0, MAX_TABS);

  const captured = await Promise.all(
    tabs.map(async (tab) => ({
      url: tab.url,
      active: tab.id === active.id,
      state: await readPageState(tab),
    }))
  );

  reply(message.id, { tabs: captured, tabIds: tabs.map((tab) => tab.id), wholeWindow });
}

async function readPageState(tab) {
  try {
    const [result] = await chrome.scripting.executeScript({ target: { tabId: tab.id }, func: capturePage });
    const state = result && result.result;
    if (!state) {
      return null;
    }
    if (JSON.stringify(state).length <= MAX_STATE_PER_TAB) {
      return state;
    }
    // too much typed text to carry: the position is still worth having
    return { scrollX: state.scrollX, scrollY: state.scrollY, video: state.video, fields: [] };
  } catch (e) {
    return null; // a page we can't look into, or a tab the browser has put to sleep
  }
}

function closeTabs(tabIds, attempt) {
  chrome.tabs.remove(tabIds).catch(() => {
    // refused while a tab is being dragged; give the drag a moment to end
    if (attempt < 4) {
      setTimeout(() => closeTabs(tabIds, attempt + 1), 400);
    }
  });
}

async function restore(message) {
  const tabs = (message.tabs || []).filter((tab) => isWebPage(tab.url)).slice(0, MAX_TABS);
  if (tabs.length === 0) {
    return;
  }

  if (message.wholeWindow && tabs.length > 1) {
    const window = await chrome.windows.create({ url: tabs.map((tab) => tab.url), focused: true });
    window.tabs.forEach((created, index) => {
      restoreWhenLoaded(created.id, tabs[index]);
      if (tabs[index].active) {
        chrome.tabs.update(created.id, { active: true });
      }
    });
    return;
  }

  const current = await chrome.windows.getLastFocused().catch(() => null);
  for (const tab of tabs) {
    const created = await chrome.tabs.create({ url: tab.url, active: true, windowId: current ? current.id : undefined });
    restoreWhenLoaded(created.id, tab);
    await chrome.windows.update(created.windowId, { focused: true });
  }
}

function restoreWhenLoaded(tabId, tab) {
  if (!tab.state) {
    return;
  }
  const expected = new URL(tab.url).origin;
  const listener = (id, info, updated) => {
    if (id !== tabId || info.status !== 'complete') {
      return;
    }
    chrome.tabs.onUpdated.removeListener(listener);
    // a redirect to somewhere else (a login page, say) must not get what was typed
    if (updated.url && new URL(updated.url).origin === expected) {
      chrome.scripting.executeScript({ target: { tabId }, func: restorePage, args: [tab.state] }).catch(() => {});
    }
  };
  chrome.tabs.onUpdated.addListener(listener);
  setTimeout(() => chrome.tabs.onUpdated.removeListener(listener), 60000);
}

// ---- these two run inside the page, so they can't use anything from this file

function capturePage() {
  const MAX_FIELDS = 200;
  const MAX_VALUE = 20000;
  const TEXT_TYPES = ['', 'text', 'search', 'email', 'url', 'tel', 'number', 'date', 'datetime-local', 'month', 'week', 'time', 'color', 'range'];
  const CHOICE_TYPES = ['checkbox', 'radio'];

  const selectorOf = (element) => {
    if (element.id) {
      return '#' + CSS.escape(element.id);
    }
    const path = [];
    for (let node = element; node && node.nodeType === 1 && node !== document.documentElement; node = node.parentElement) {
      if (node.id) {
        path.unshift('#' + CSS.escape(node.id));
        break;
      }
      let index = 1;
      for (let sibling = node.previousElementSibling; sibling; sibling = sibling.previousElementSibling) {
        if (sibling.localName === node.localName) {
          index++;
        }
      }
      path.unshift(node.localName + ':nth-of-type(' + index + ')');
    }
    return path.join('>');
  };

  // never passwords, card details or one-time codes
  const isPrivate = (element) => {
    const autocomplete = (element.getAttribute('autocomplete') || '').toLowerCase();
    return /(^|\s)(cc-|current-password|new-password|one-time-code)/.test(autocomplete);
  };

  const fields = [];
  for (const element of document.querySelectorAll('input,textarea,select')) {
    if (fields.length >= MAX_FIELDS) {
      break;
    }
    if (element.disabled || isPrivate(element)) {
      continue;
    }

    const name = element.localName;
    if (name === 'input' && CHOICE_TYPES.includes(element.type)) {
      if (element.checked !== element.defaultChecked) {
        fields.push({ selector: selectorOf(element), checked: element.checked });
      }
    } else if (name === 'select') {
      const changed = Array.from(element.options).some((option) => option.selected !== option.defaultSelected);
      if (changed) {
        fields.push({ selector: selectorOf(element), selectedIndex: element.selectedIndex });
      }
    } else if (name === 'textarea' || TEXT_TYPES.includes(element.type)) {
      if (element.value !== element.defaultValue && element.value.length <= MAX_VALUE) {
        fields.push({ selector: selectorOf(element), value: element.value });
      }
    }
  }

  let video = null;
  const videos = Array.from(document.querySelectorAll('video')).filter((item) => item.duration > 0);
  const main = videos.find((item) => !item.paused) || videos.sort((a, b) => b.duration - a.duration)[0];
  if (main) {
    video = { time: main.currentTime, paused: main.paused };
  }

  return { scrollX: window.scrollX, scrollY: window.scrollY, fields, video };
}

function restorePage(state) {
  const number = (value) => (typeof value === 'number' && isFinite(value) ? value : 0);

  // pages often grow while they load, so one scroll may fall short
  let tries = 0;
  const scroll = () => {
    window.scrollTo(number(state.scrollX), number(state.scrollY));
    if (++tries < 8 && Math.abs(window.scrollY - number(state.scrollY)) > 2) {
      setTimeout(scroll, 400);
    }
  };
  scroll();

  // the setter on the prototype, so that frameworks like React notice the change
  const setValue = (element, value) => {
    const prototype = element.localName === 'textarea' ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
    Object.getOwnPropertyDescriptor(prototype, 'value').set.call(element, value);
  };

  for (const field of Array.isArray(state.fields) ? state.fields : []) {
    let element = null;
    try {
      element = document.querySelector(String(field.selector));
    } catch (e) {
      continue;
    }
    if (!element || element.type === 'password' || element.type === 'file') {
      continue;
    }
    if (typeof field.value === 'string' && (element.localName === 'input' || element.localName === 'textarea')) {
      setValue(element, field.value);
    } else if (typeof field.checked === 'boolean') {
      element.checked = field.checked;
    } else if (typeof field.selectedIndex === 'number' && element.localName === 'select') {
      element.selectedIndex = field.selectedIndex;
    } else {
      continue;
    }
    element.dispatchEvent(new Event('input', { bubbles: true }));
    element.dispatchEvent(new Event('change', { bubbles: true }));
  }

  if (state.video) {
    const applyVideo = (video) => {
      video.currentTime = number(state.video.time);
      if (state.video.paused === false) {
        video.play().catch(() => {});
      }
    };
    const find = () => {
      const video = document.querySelector('video');
      if (video && video.readyState > 0) {
        applyVideo(video);
        return true;
      }
      return false;
    };
    if (!find()) {
      let attempts = 0;
      const timer = setInterval(() => {
        if (find() || ++attempts > 40) {
          clearInterval(timer);
        }
      }, 250);
    }
  }
}
