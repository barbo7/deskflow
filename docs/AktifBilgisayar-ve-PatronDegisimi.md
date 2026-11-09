# Aktif bilgisayar göstergesi ve otomatik patron değişimi

> **English summary:** This branch (`feat/active-computer-indicator`, based on Deskflow 1.27.0) adds:
> an indicator of which computer has the cursor (main window and tray), "Switch to" buttons,
> arrow hotkeys, and **automatic role swaps**: pushing the *client's* own mouse against the edge
> that leads to the server (or pressing ctrl+alt+super+arrow on its keyboard) makes the client
> take over as server, so both computers' mice and keyboards can reach the other one while
> clipboard sharing keeps working. It also adds safety nets (an escape hotkey, a 45 s revert,
> game protection). Protocol version 1.9; older peers negotiate down to 1.8 and are unaffected.

Bu belge, bir Mac ile bir Windows bilgisayarı **tek fare ve klavyeyle** kullanmak için Deskflow'a
neler eklediğimizi, nasıl çalıştığını ve başka bilgisayarlara nasıl kurulacağını anlatır.

---

## 1. Neden yaptık?

**İhtiyaç:** Mac (MacBook Air) ve Windows (Barbo) yan yana duruyor. İkisini de hangi bilgisayarın
başındaysak **o bilgisayarın faresi ve klavyesiyle** kullanmak istiyoruz. İş yaparken
kopyala-yapıştır çalışmalı; oyun oynarken de fare yanlışlıkla öbür ekrana kaçmamalı.

**Baktığımız seçenekler:**

| Seçenek | Neden yetmedi |
|---|---|
| Deskflow (resmi) | Sadece **sunucunun** ("patron") faresi öbür bilgisayara geçebiliyor. Windows faresiyle Mac'e geçemiyoruz |
| Rolleri elle değiştirmek | Her seferinde iki bilgisayarda ayar değiştirmek gerekiyor; pratik değil |
| Lan Mouse | İki yönlü çalışıyor ama **kopyala-yapıştır yok** |
| Synergy / ShareMouse | Ücretli ve aynı tek-patron sınırı |

**Karar:** Deskflow açık kaynak (GPL). Kopyala-yapıştır gibi olgun özellikleri korumak için
Deskflow'un içine **otomatik patron değişimi** ekledik: hangi bilgisayarın faresi kenara itilirse
o bilgisayar kendiliğinden patron oluyor.

---

## 2. Neler eklendi?

| Özellik | Ne yapar |
|---|---|
| **Aktif bilgisayar göstergesi** | Deskflow penceresinde "In use: Barbo (Windows)" satırı; tepsi/menü çubuğu simgesi elma 🍎 ya da dört kare ⊞ olur |
| **Switch to düğmeleri** | Pencereden ve tepsi menüsünden tek tıkla öbür bilgisayara geçiş. İmleç hedef ekranın **ortasına** gider |
| **Ok tuşlu kısayollar** | `Ctrl+Alt+Win+←/→` (Windows), `Control+Option+Command+←/→` (Mac) |
| **Otomatik patron değişimi** | Yardımcı bilgisayarın **kendi faresi** patrona bakan kenara itilince (ya da kendi klavyesinde kısayola basılınca) roller değişir, ~1-4 saniye sürer |
| **Acil dönüş tuşu** | `Ctrl+Alt+Win+M` / `Control+Option+Command+M`: fare ve klavye her durumda anında patrona döner |
| **45 saniyelik geri dönüş** | Patron değişimi tamamlanamazsa iki bilgisayar da kendiliğinden eski rolüne döner |
| **Kontrol edilemeyen ekran koruması** | Windows'ta yönetici izni (UAC) veya kilit ekranı açılırsa imleç kendiliğinden patrona döner |
| **Oyun koruması** | Bir oyun imleci gizler ya da pencereye hapsederse, kenardan geçiş ve patron değişimi **kapanır** |

### Kısayol tablosu

Yerleşim: **Windows solda, Mac sağda.**

| Nerede | Tuş | Sonuç |
|---|---|---|
| Patronun klavyesi | `…+←` / `…+→` | İmleç o yöndeki bilgisayara **anında** geçer |
| Yardımcının klavyesi | Patrona doğru ok (`…+→` Windows'ta, `…+←` Mac'te) | Önce patron değişimi, sonra imleç karşıya geçer |
| Her yerde | `…+M` | Acil dönüş: imleç patrona döner |
| Patron Windows iken | **Scroll Lock** | İmleci Windows'a kilitler (Deskflow'un kendi özelliği) |

`…` = Windows'ta `Ctrl + Alt + Win`, Mac'te `Control + Option + Command`.

---

## 3. Nasıl çalışıyor?

### Genel akış

```
 Windows (yardımcı)                     Mac (patron)
 ──────────────────                     ────────────
 Fare sağ kenara itildi
 (gerçek fare, Deskflow'un kendi
  hareketi değil; oyun yok)
        │
        ├── DSRQ "patron olmak istiyorum,
        │         IP adreslerim: …" ─────────▶ isteği kaydeder
        │                                      DSGO "tamam" ──────┐
        ◀──────────────────────────────────────────────────────────┘
 Pencere: Server moduna geç,
 Mac'in gönderdiği yerleşimi kullan,
 bilgisayar listesine Mac'i ekle,
 core'u yeniden başlat
        │
        ×── bağlantı kopar ─────────────────▶ "yardımcı bağlantıyı kesti":
                                               Pencere: Client moduna geç,
                                               Windows'un adresine bağlan
        ◀──────────────────── Mac bağlanır ────────┘
 Patron Windows; imleci Mac'e taşır
```

Önemli tasarım kararı: Mac, **yardımcı bağlantıyı kendisi kesene kadar** rolünü bırakmaz. İlk
sürümde Mac "tamam" der demez kapanıyordu; mesaj yerine ulaşamadığında iki bilgisayar da yardımcı
kalıp kilitleniyordu.

### Protokol (sürüm 1.9)

| Mesaj | Yön | İçerik |
|---|---|---|
| `DACT` | Patron → yardımcı | Aktif bilgisayar + bağlı bilgisayarlar (`ad|platform`) |
| `DSWR` | Yardımcı → patron | "Şu bilgisayara geç" (boş ad = patronun kendisi) |
| `DPLT` | Yardımcı → patron | Platform: `mac`, `windows`, `linux` |
| `DSWI` | Patron → yardımcı | Patrona bakan kenarlar + patronun yerleşimi |
| `DSRQ` | Yardımcı → patron | Patron olma isteği + yardımcının IP adresleri |
| `DSGO` | Patron → yardımcı | İstek kabul edildi |

Eski Deskflow sürümleri 1.8'e düşer; bu mesajları hiç görmezler.

### Kodda nerede?

| Parça | Dosya |
|---|---|
| Mesaj tanımları | `src/lib/deskflow/ProtocolTypes.{h,cpp}` |
| Patron tarafı | `src/lib/server/Server.cpp`, `ClientProxy1_9.{h,cpp}` |
| Yardımcı tarafı | `src/lib/client/ServerProxy1_9.{h,cpp}`, `Client.cpp`, `src/lib/deskflow/ClientApp.cpp` |
| Kenar itme algılama | `src/lib/platform/OSXEdgePushDetector.mm`, `MSWindowsEdgePushDetector.cpp` |
| "Ne kadar itince" mantığı | `src/lib/deskflow/EdgePushAccumulator.h` (testli) |
| Oyun algılama | `MSWindowsComputer::isMouseCapturedByApp` |
| Pencere: rol değişimi, gösterge | `src/lib/gui/MainWindow.cpp`, `core/CoreProcess.cpp`, `ActiveComputerIcon.cpp` |
| Çekirdek ↔ pencere mesajları | `src/lib/deskflow/ipc/CoreIpc*.{h,cpp}` |

### Algılama nasıl yanlış tetiklenmiyor?

- Sadece **gerçek** fare sayılır: Deskflow'un kendi gönderdiği hareketler yok sayılır
  (Windows: aygıtı olmayan raw input; Mac: kendi süreç kimliğinden gelen olaylar).
- Kenar **boyunca** hareket (ör. kaydırma çubuğu) sayılmaz; kenara **doğru itmek** gerekir
  (Mac ~80 nokta, Windows ~200 fare birimi, 0,4 sn içinde).
- Bir tetiklemeden sonra 5 saniye yeni tetikleme yok.
- Oyun imleci hapsetmişse hiç tetiklenmez.

### Güvenlik

- Bağlantı TLS ile şifreli kalır. Rol değişiminde, öbür bilgisayarın **zaten güvenilen** parmak
  izi yeni rol için de güvenilir sayılır; böylece her ekranda onay penceresi çıkmaz.
- Patron, kendi dinleme adresini yerleşimle birlikte göndermez.

---

## 4. Başka bilgisayarlara kurulum

### Windows

1. Bu dalın son GitHub Actions derlemesinden `package-deskflow-windows-2022-x64` dosyasını indir
   (Actions sekmesi → son başarılı "Continuous Integration" → Artifacts). Ya da:
   ```
   gh run download <RUN_ID> -R barbo7/deskflow -n package-deskflow-windows-2022-x64
   ```
2. Çalışan Deskflow'u kapat, `.msi`'yi kur (imzasız: "Bilinmeyen yayıncı" uyarısı normal).
3. **Ayarlarda süreç modunu Desktop yap** (Service değil; bkz. bilinen sorunlar).
4. Client modunda Mac'in IP adresine bağlan.

> GitHub Actions dosyaları **90 gün** sonra silinir. Kalıcı olsun istersen bir GitHub Release oluştur.

### Mac (kaynaktan derleme)

```bash
brew install cmake ninja qt openssl@3 pkgconf
git clone -b feat/active-computer-indicator https://github.com/barbo7/deskflow.git
cd deskflow
cmake -S. -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix openssl@3)" -DBUILD_INSTALLER=OFF
cmake --build build
```

Sonra uygulamaya resmi Deskflow'dan **farklı bir kimlik** ver ve yerel olarak imzala; yoksa macOS
Erişilebilirlik iznini resmi sürümle karıştırır:

```bash
cp -R build/bin/Deskflow.app "/Applications/Deskflow Dev.app"
P="/Applications/Deskflow Dev.app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier org.deskflow.deskflow.dev" "$P"
/usr/libexec/PlistBuddy -c "Set :CFBundleName Deskflow Dev" "$P"
codesign --force --deep -s - "/Applications/Deskflow Dev.app"
```

Ardından Sistem Ayarları → Gizlilik ve Güvenlik → Erişilebilirlik → ＋ → **Deskflow Dev**.

> Her yeni derlemede macOS izni yeniden ister. Uygulamayı **yeni bir adla** (ör. "Deskflow Dev 2")
> kurmak, eski izin kayıtlarıyla karışmayı önler.

### İki bilgisayarda da

- İkisi de bu daldan derlenmiş olmalı (protokol 1.9). Biri eski sürümse gösterge ve geçiş
  düğmeleri çalışır ama patron değişimi çalışmaz.
- Bilgisayar adları her iki tarafta aynı olmalı (ör. `Barbo`, `BORA-MacBook-Air.local`).

---

## 5. Bilinen sorunlar ve sınırlar

| Durum | Açıklama |
|---|---|
| **Windows Service modu** | Bu modda pencere, "durdur/başlat" komutunu servise iletemiyor; patron değişimi yarım kalıp 45 sn sonra geri dönüyor. **Desktop modu kullan.** Sebebi henüz kesin bulunamadı |
| Desktop modunun eksiği | Mac patronken, Windows'ta yönetici olarak çalışan pencereler (Görev Yöneticisi gibi) Mac faresiyle kontrol edilemez |
| İlk değişimde parmak izi | Bilgisayarlar birbirine hiç güvenmemişse ilk değişimde onay penceresi çıkabilir |
| Sadece 2 bilgisayarla denendi | Daha fazla bilgisayarlı yerleşimler test edilmedi |
| Linux | Kenar itme algılayıcısı yok; gösterge ve düğmeler çalışır |
| Mac menü çubuğu simgesi | Kamera çentikli Mac'lerde kalabalık menü çubuğunda gizlenebilir; pencere üzerindeki satır her zaman görünür |
| Resmi güncellemeler | Resmi Deskflow güncellenince bu değişikliklerin yeni sürüme taşınması gerekir |

## 6. Sorun giderme

| Belirti | Ne yap |
|---|---|
| Fare/klavye bir bilgisayarda takıldı | Acil dönüş: `…+M` |
| Patron değişimi yarım kaldı | 45 sn bekle, iki taraf kendiliğinden döner |
| Değişim hiç olmuyor | Deskflow penceresinin Log panelinde `role swap` satırlarına bak |
| Mac uygulaması açılıp hemen kapanıyor | Erişilebilirlik izni eksik; ＋ ile ekle |

---

## 7. Commit geçmişi

| Commit | Ne |
|---|---|
| `9ef60c5` | Aktif bilgisayar göstergesi ve "Switch to" (protokol 1.9: DACT, DSWR, DPLT) |
| `412047e` | Otomatik patron değişimi (DSWI, DSRQ, DSGO, kenar itme algılayıcıları) |
| `f7abad0` | Güvenli başarısızlık: bağlantı kopana kadar bekleme, 45 sn geri dönüş, acil dönüş tuşu, UAC koruması |
| `d4f84ac` | Oyun koruması |
| `3abdc37` | Ok tuşlu kısayollar, değişimden sonra imleci karşıya taşıma, yerleşim dosyası düzeltmeleri |
| `26f31a0` | Service modu IPC yeniden bağlanma, bilgisayar listesini ayarlara ekleme |
