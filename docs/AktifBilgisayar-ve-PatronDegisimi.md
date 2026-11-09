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

**İhtiyaç:** Bir Mac ve bir Windows bilgisayar yan yana duruyor. İkisini de hangi bilgisayarın
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
| **Aktif bilgisayar göstergesi** | Deskflow penceresinde "In use: WindowsPC (Windows)" satırı; tepsi/menü çubuğu simgesi elma 🍎 ya da dört kare ⊞ olur |
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

Hazır kurulum dosyaları: **[Release roleswap-1](https://github.com/barbo7/deskflow/releases/tag/roleswap-1)**

En kolay yol: aşağıdaki metni **iki bilgisayardaki Claude Code'a ayrı ayrı** yapıştırmak.

```text
İki bilgisayar arasında tek fare ve klavye paylaşımı kuracağız. Deskflow'un gayriresmi "roleswap"
sürümünü kullanacağız: hangi bilgisayarın faresi kenara itilirse o bilgisayar patron (server) olur.
Bu metni iki bilgisayara da yapıştırıyorum; sen sadece bu bilgisayarla ilgilen.

Kaynaklar:
- Release: https://github.com/barbo7/deskflow/releases/tag/roleswap-1
- Belge: https://github.com/barbo7/deskflow/blob/feat/active-computer-indicator/docs/AktifBilgisayar-ve-PatronDegisimi.md

Kurallar:
- Her adımı kısa ve Türkçe anlat. Bir şey ters giderse tahmin yürütme, dur ve bana göster.
- Yalnızca Deskflow'a dokun. Yönetici izni, Erişilebilirlik izni ve parmak izi onaylarını ben veririm.
- Başlamadan önce bana sor: bu bilgisayar ilk kurulumda SERVER mı CLIENT mı olacak, öbür bilgisayar
  bu bilgisayarın hangi tarafında duruyor?

Adımlar:
1. İşletim sistemini ve işlemci mimarisini bul. Release'ten uygun dosyayı ve sums.txt'yi indir:
   Windows x64: deskflow-roleswap-1-win-x64.msi
   Windows ARM: deskflow-roleswap-1-win-arm64.msi
   Apple Silicon Mac: deskflow-roleswap-1-macos-arm64.dmg
   Intel Mac: deskflow-roleswap-1-macos-x86_64.dmg
2. Dosyanın SHA-256 özetini sums.txt ile karşılaştır. Uyuşmazsa dur.
3. Çalışan Deskflow varsa kapat.
4. Kur:
   - Windows: msiexec /i ile kur. Dosya imzasız, "Bilinmeyen yayıncı" uyarısı normal.
   - Mac: dmg'yi aç, uygulamayı /Applications/Deskflow Roleswap.app olarak kopyala, sonra sırayla:
     * Info.plist içinde CFBundleIdentifier değerini org.deskflow.deskflow.roleswap yap
       (resmi Deskflow ile izinleri karışmasın diye)
     * xattr -dr com.apple.quarantine ile karantina işaretini kaldır
     * codesign --force --deep -s - ile yerel olarak imzala ve codesign --verify --deep ile doğrula
     Sonra Sistem Ayarları > Gizlilik ve Güvenlik > Erişilebilirlik sayfasını aç;
     ＋ ile uygulamayı eklememi ve anahtarını açmamı iste.
5. Windows ise Deskflow ayarlarında süreç modunu Desktop yap (Service değil). Service modunda patron
   değişimi çalışmıyor.
6. Deskflow'u aç ve bu bilgisayarın adını ve IP adresini bana söyle.
7. SERVER ise: Server modunu seç, Configure Server ekranında öbür bilgisayarı doğru tarafa, adını
   birebir aynı yazarak yerleştir, Deskflow'u başlat.
   CLIENT ise: Client modunu seç, server'ın IP adresini bana sor ve yaz, Deskflow'u başlat.
8. İlk bağlantıda çıkan parmak izi pencerelerini onaylamamı iste. Bağlanınca bana haber ver.

Deneme (iki bilgisayar da hazır olunca):
- Server'ın faresiyle ekran kenarından öbür bilgisayara geç.
- Client'ın kendi faresini server'a bakan kenara doğru it. 1-5 saniye içinde roller değişmeli ve
  imleç öbür bilgisayara geçmeli.
- Kısayol: Ctrl+Alt+Win+ok (Windows), Control+Option+Command+ok (Mac).
- Acil dönüş: aynı tuşlar + M.
- Değişim başarısız olursa 45 saniye içinde iki taraf eski rolüne döner. Deskflow penceresinin Log
  panelindeki "role swap" satırlarını bana getir.
```

### Elle kurmak isteyenler

Yukarıdaki adımların aynısını elle uygulayabilirsin. Mac'te kaynaktan derlemek için:

```bash
brew install cmake ninja qt openssl@3 pkgconf
git clone -b feat/active-computer-indicator https://github.com/barbo7/deskflow.git
cd deskflow
cmake -S. -Bbuild -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix openssl@3)" -DBUILD_INSTALLER=OFF
cmake --build build
```

Kaynaktan derlenen uygulama Homebrew'deki Qt'ye bağlıdır; başka Mac'lere kopyalamak için Release'teki
`.dmg` dosyasını kullan.

> Mac'te her yeni sürüm için Erişilebilirlik izni yeniden gerekir. macOS aynı adlı uygulamayı
> önbellekte tutabildiği için yeni sürümü **yeni bir adla** kurmak en sorunsuz yoldur.

### İki bilgisayarda da

- İkisi de bu sürümde olmalı. Biri resmi Deskflow ise paylaşım çalışır, ama patron değişimi çalışmaz.
- Bilgisayar adları her iki tarafta aynı olmalı (ör. `WindowsPC`, `MacBook.local`).

---

## 5. Bilinen sorunlar ve sınırlar

| Durum | Açıklama |
|---|---|
| **Windows Service modu** | Bu modda pencere, "durdur/başlat" komutunu servise iletemiyor; patron değişimi yarım kalıp 45 sn sonra geri dönüyor. **Desktop modu kullan.** Sebebi henüz kesin bulunamadı |
| Desktop modunun eksiği | Mac patronken, Windows'ta yönetici olarak çalışan pencereler (Görev Yöneticisi gibi) Mac faresiyle kontrol edilemez |
| İlk değişimde parmak izi | Bilgisayarlar birbirine hiç güvenmemişse ilk değişimde onay penceresi çıkabilir |
| Test kapsamı | Bir Mac ile bir Windows x64 bilgisayarda denendi. Üç veya daha fazla bilgisayar, Windows ARM ve Release'teki Mac `.dmg` dosyaları denenmedi |
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

## 7. Monitör düzeni ve uygulama taşıma (protokol 1.11)

### Monitör düzeni

Configure Server > **Monitors** sekmesi her bilgisayarın gerçek monitörlerini ölçekli gösterir; kimin
bağlı olduğu ve imlecin nerede olduğu canlı güncellenir. "Arrange computers by their monitors"
kutusu işaretlenince bilgisayarlar sürüklenebilir, yaklaştıkları kenara yapışırlar.

- İmleç yalnızca **yeşil çizgiler** boyunca geçer. Çizgiler, iki bilgisayarın gerçekten monitörü olan
  kenarlarının değdiği yerlerdir.
- Sadece yerleşim değiştiyse **bağlantı kopmadan** uygulanır. Bilgisayar eklemek, kısayol ya da genel
  ayar değiştirmek eskisi gibi yeniden başlatır.
- Monitör takılıp çıkarılınca çizgiler kendiliğinden yenilenir.
- Bir bilgisayarın **kendi içindeki** monitör sırası işletim sisteminin ayarıdır; Deskflow bunu
  değiştirmez. Taşınan birim bilgisayarın tamamıdır.

### Uygulama taşıma

**Fareyle:** pencereyi başlığından (tarayıcıda sekmesinden) tut, öbür bilgisayara bakan kenara götür ve
tuşu bırakmadan **kenara doğru itmeye devam et**. Uygulama karşıda açılır, imleç de oraya geçer. Kenara
sadece değmek yetmez, böylece Windows'un yarım ekran yerleştirmesi bozulmaz. Kaynak pencere, karşı taraf açtıktan sonra
kapanır.

Metin seçerken ya da dosya sürüklerken kenara itmek bir şey yapmaz: yalnızca başlık çubuğundan tutulan
pencere ve tarayıcı sekmesi sayılır. Mac pencerenin sürüklendiğini söyleyemediği için orada imlecin
pencerenin üst şeridinde olmasına bakılır.

**Kısayolla:** öndeki uygulamayı bir sonraki bilgisayara gönderir.

| Kısayol | Nerede |
|---|---|
| `Ctrl+Alt+Win+H` / `Control+Option+Command+H` | Patronun klavyesinde: imlecin olduğu bilgisayardaki uygulama taşınır |
| Aynı tuşlar | Yardımcının kendi klavyesinde: o bilgisayardaki uygulama taşınır |

Çalışan bir program Mac ile Windows arasında taşınamaz. Taşınan şey **sayfa ya da belgedir**; karşıda
aynı uygulama açılır.

| Ne | Ne olur |
|---|---|
| Chrome, Edge, Brave, Safari | Sayfa karşıda açılır, sekme kaynakta kapanır (`Ctrl+Shift+T` ile geri gelir) |
| Word, Excel, PowerPoint, Acrobat | Belgenin **diske kayıtlı hali** kopyalanır (`İndirilenler/Deskflow Handoff`), karşıda açılır, kaynaktaki pencere kapanır |
| VS Code, Cursor, Sublime | Aynısı, ama kaynakta yalnızca o sekme kapanır |
| Spotify, Slack, Discord, Teams, Zoom, Notion, Figma... | Uygulama karşıda açılır, kaynaktaki penceresi kapanır; içeriği kendi hesabından gelir |
| Listede olmayan uygulama | Belgesi varsa ve düz bir belgeyse varsayılan uygulamayla açılır, yoksa taşınmaz |

Sınırlar:

- Kaynak, ancak karşı taraf "açtım" dedikten sonra kapanır. Karşıda açılamazsa ya da sayfa/belge
  gönderilemediyse kaynakta kalır.
- Kapatma, pencerenin kendi kapatma düğmesiyle aynıdır: kaydedilmemiş iş varsa uygulama sorar.
- Kaydedilmemiş değişiklikler ve açık sekme durumu taşınmaz.
- En fazla 32 MB'lık dosya gider; daha büyüğünde yalnızca uygulama açılır.
- Firefox'un Mac'te betik arayüzü yok: Mac'ten gönderilirken sayfa gitmez, yalnızca Firefox açılır.
- Windows'ta açık belge pencere başlığından ve "Son kullanılanlar" klasöründen bulunur; dosya orada
  yoksa yalnızca uygulama açılır.
- Mac'te ilk kullanımda "Deskflow, Chrome'u denetlemek istiyor" izni sorulur.
- Linux yalnızca alabilir.

Güvenlik: karşı taraf yalnızca listedeki bir uygulamanın **kimliğini** gönderebilir, çalıştırılacak bir
program gönderemez. Adres olarak yalnızca `http(s)`, varsayılan uygulamayla açmak için yalnızca düz belge
türleri kabul edilir; dosya adları temizlenir.

| Mesaj | Yön | İçerik |
|---|---|---|
| `DMON` | Yardımcı → patron | Monitörler (`x,y,g,y;...`) |
| `DHRQ` | Patron → yardımcı | "Öndeki uygulamayı şuna gönder" |
| `DHDR` | Patron → yardımcı | "Sürüklenen bir pencere varsa şuna gönder" |
| `DHST` | İki yönlü | Hedef, uygulama, adres, dosya adı, boyut |
| `DHDT` | İki yönlü | Dosyanın bir parçası (512 KB) |
| `DHAK` | İki yönlü (1.11) | Karşı taraf açabildi mi |

Eski sürümler uygun protokole düşer ve bu özellikleri görmez. Karşı taraf 1.10 ise taşıma çalışır ama
"açtım" onayı gelmediği için kaynak kapanmaz. Her şey için iki bilgisayar da bu sürümde olmalı.

---

## 8. Commit geçmişi

| Commit | Ne |
|---|---|
| `9ef60c5` | Aktif bilgisayar göstergesi ve "Switch to" (protokol 1.9: DACT, DSWR, DPLT) |
| `412047e` | Otomatik patron değişimi (DSWI, DSRQ, DSGO, kenar itme algılayıcıları) |
| `f7abad0` | Güvenli başarısızlık: bağlantı kopana kadar bekleme, 45 sn geri dönüş, acil dönüş tuşu, UAC koruması |
| `d4f84ac` | Oyun koruması |
| `3abdc37` | Ok tuşlu kısayollar, değişimden sonra imleci karşıya taşıma, yerleşim dosyası düzeltmeleri |
| `26f31a0` | Service modu IPC yeniden bağlanma, bilgisayar listesini ayarlara ekleme |
