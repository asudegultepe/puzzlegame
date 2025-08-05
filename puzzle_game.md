# Unreal Engine Puzzle Oyunu

## Proje Yapısı ve Başlangıç

### 1. Proje Kurulumu
- Yeni bir Unreal Engine projesi oluşturun (Third Person veya Blank template)
- GitHub reposu oluşturun ve projeyi bağlayın
- Temel klasör yapısını organize edin:
  - `/Source/[ProjeAdı]/` - C++ kodları
  - `/Content/Blueprints/` - Blueprint dosyaları
  - `/Content/UI/` - Widget'lar
  - `/Content/Materials/` - Materyal dosyaları
  - `/Content/Meshes/` - 3D modeller

### 2. Temel Sistem Mimarisi Planlama
- **GameMode**: Oyunun ana mantığını yönetecek
- **GameState**: Oyun durumunu (süre, hamle sayısı) takip edecek
- **PlayerController**: Oyuncu girişlerini yönetecek
- **PuzzlePiece**: Her puzzle parçası için C++ sınıfı
- **PuzzleWidget**: Ana UI widget'ı
- **GameHUD**: Oyun sırasındaki bilgileri gösterecek

## Adım 1: Temel C++ Sınıflarının Oluşturulması

### PuzzlePiece Sınıfı (C++)
```cpp
// PuzzlePiece.h
UCLASS()
class PUZZLEGAME_API APuzzlePiece : public AActor
{
    GENERATED_BODY()

public:
    APuzzlePiece();

protected:
    // Puzzle parçasının mesh komponenti
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PieceMesh;

    // Parçanın ID'si
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    int32 PieceID;

    // Doğru pozisyon
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
    FVector CorrectPosition;

    // Parçanın doğru konumda olup olmadığı
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    bool bIsInCorrectPosition;

public:
    // Parçanın doğru konumda olup olmadığını kontrol et
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CheckIfInCorrectPosition();

    // Parçayı belirli bir konuma taşı
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    void MovePieceToLocation(FVector NewLocation);
};
```

### PuzzleGameMode Sınıfı (C++)
```cpp
// PuzzleGameMode.h
UCLASS()
class PUZZLEGAME_API APuzzleGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APuzzleGameMode();

protected:
    // Oyun süresi
    UPROPERTY(BlueprintReadOnly, Category = "Game Stats")
    float GameTime;

    // Toplam hamle sayısı
    UPROPERTY(BlueprintReadOnly, Category = "Game Stats")
    int32 TotalMoves;

    // Puzzle parçalarının listesi
    UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
    TArray<APuzzlePiece*> PuzzlePieces;

public:
    // Hamle sayısını artır
    UFUNCTION(BlueprintCallable, Category = "Game Stats")
    void IncrementMoveCount();

    // Oyunun bitip bitmediğini kontrol et
    UFUNCTION(BlueprintCallable, Category = "Puzzle")
    bool CheckGameCompletion();

    // Oyunu başlat
    UFUNCTION(BlueprintCallable, Category = "Game")
    void StartGame();
};
```

## Adım 2: UI Sisteminin Oluşturulması

### Ana Puzzle Widget'ı (Blueprint)
1. **Widget Blueprint oluşturun**: `WBP_PuzzleMain`
2. **Ana bileşenler**:
   - Vertical Box (ana container)
   - Scroll Box (puzzle parçaları listesi için)
   - Panel (sürükle-bırak alanı)

### Puzzle Parçası Widget'ı (Blueprint)
1. **Widget Blueprint oluşturun**: `WBP_PuzzlePiece`
2. **Bileşenler**:
   - Image (parçanın görselini göstermek için)
   - Button (tıklama etkileşimi için)
3. **Drag & Drop fonksiyonalitesi**:
   - `OnDragDetected` event'ini override edin
   - `OnDrop` event'ini handle edin

### Oyun İstatistikleri Widget'ı (Blueprint)
1. **Widget Blueprint oluşturun**: `WBP_GameStats`
2. **Bileşenler**:
   - Text Block (süre gösterimi)
   - Text Block (hamle sayısı)
   - Progress Bar (tamamlanma oranı - opsiyonel)

## Adım 3: Sürükle-Bırak Sisteminin Implementasyonu

### UI'dan Sahneye Sürükleme
1. **Widget'ta Drag Detection**:
   - `OnMouseButtonDown` event'inde sürükleme başlat
   - Mouse pozisyonunu takip et
   - Sahne koordinatlarına dönüştür

2. **3D Parça Oluşturma**:
   - PlayerController'da line trace kullanarak sahne pozisyonu bul
   - Belirlenen pozisyonda PuzzlePiece spawn et
   - Widget'tan 3D objeye geçişi animate et

### Sahnede Sürükle-Bırak
1. **Mouse Input Handling**:
   - PlayerController'da mouse input'larını yakala
   - Hit test ile puzzle parçalarını detect et
   - Seçili parçayı mouse ile birlikte hareket ettir

2. **Parça Değiştirme Sistemi**:
   - İki parça üst üste geldiğinde pozisyon değiştirme
   - Smooth transition animasyonları
   - Collision detection

## Adım 4: Oyun Mekaniklerinin Implementasyonu

### Rastgele Sıralama Sistemi
```cpp
// GameMode'da puzzle parçalarını karıştır
void APuzzleGameMode::ShufflePuzzlePieces()
{
    // Array'i random olarak karıştır
    for (int32 i = PuzzlePieces.Num() - 1; i > 0; i--)
    {
        int32 RandomIndex = FMath::RandRange(0, i);
        PuzzlePieces.Swap(i, RandomIndex);
    }
}
```

### Süre ve Hamle Takibi
1. **Timer System**:
   - GameMode'da Timer handle kullan
   - Her saniye UI'ı güncelle
   - Blueprint'ta binding ile otomatik güncelleme

2. **Hamle Sayacı**:
   - Her parça hareketi sonrası increment
   - UI'da real-time güncelleme

### Doğru Pozisyon Kontrolü
```cpp
bool APuzzlePiece::CheckIfInCorrectPosition()
{
    float DistanceThreshold = 50.0f; // Tolerans mesafesi
    float Distance = FVector::Dist(GetActorLocation(), CorrectPosition);
    bIsInCorrectPosition = Distance <= DistanceThreshold;
    return bIsInCorrectPosition;
}
```

## Adım 5: Bitiş Ekranı ve Oyun Tamamlama

### Tamamlama Kontrolü
1. **Her hamle sonrası kontrol**:
   - Tüm parçaların doğru pozisyonda olup olmadığı
   - GameMode'da CheckGameCompletion fonksiyonu
   - Event dispatcher ile UI'a bildirim

### Bitiş Ekranı Widget'ı
1. **Widget Blueprint**: `WBP_GameComplete`
2. **İçerik**:
   - Tebrik mesajı
   - Toplam süre gösterimi
   - Toplam hamle sayısı
   - Yeniden oyna butonu

## Adım 6: Optimizasyon ve Polish

### Performans Optimizasyonları
1. **Object Pooling**: Puzzle parçaları için
2. **LOD Sistemleri**: 3D modeller için
3. **Efficient Update Loops**: Gereksiz tick'leri kaldır

### Visual Polish
1. **Animasyonlar**:
   - Parça yerleştirme animasyonları
   - UI transition'ları
   - Feedback animasyonları

2. **Audio** (opsiyonel):
   - Parça yerleştirme sesleri
   - UI click sesleri
   - Tamamlama müziği

## Adım 7: Test ve Debug

### Test Senaryoları
1. **Temel Fonksiyonalite**:
   - UI'dan sahneye sürükleme
   - Sahnede parça hareketi
   - Parça değiştirme
   - Doğru pozisyon kontrolü

2. **Edge Case'ler**:
   - Aynı anda birden fazla parça sürükleme
   - Sahne dışına sürükleme
   - Rapid clicking/dragging

### Debug Araçları
1. **Visual Debug**:
   - Doğru pozisyonları göster
   - Collision box'ları görselleştir
   - Distance threshold'ları göster

2. **Console Commands**:
   - Oyunu hızlıca test etmek için cheat'ler
   - Debug bilgilerini toggle etme

## Teslim Öncesi Kontrol Listesi

### Kod Kalitesi
- [ ] Tüm sınıf ve fonksiyonlar için yorum satırları
- [ ] Consistent naming convention
- [ ] Error handling ve null checks
- [ ] Memory leak kontrolü

### GitHub Reposu
- [ ] Düzenli commit history
- [ ] README.md dosyası
- [ ] Build instructions
- [ ] Known issues listesi

### Oynanabilirlik
- [ ] Tüm temel özellikler çalışıyor
- [ ] Performance sorunları yok
- [ ] UI responsive ve kullanışlı
- [ ] Game feel tatmin edici

Bu rehber, projeyi baştan sona tamamlamanız için gereken tüm adımları içermektedir. Her adımı sırayla takip ederek, istenen özelliklere sahip bir puzzle oyunu geliştirebilirsiniz.

graph TB
    %% Ana Sistem Katmanları
    subgraph "C++ Core Layer"
        GM[PuzzleGameMode<br/>- GameTime tracking<br/>- Move counter<br/>- Game completion check]
        PC[PuzzlePlayerController<br/>- Mouse input handling<br/>- Drag detection<br/>- Line tracing]
        PP[PuzzlePiece<br/>- Position validation<br/>- Movement logic<br/>- ID management]
        GS[PuzzleGameState<br/>- Current game stats<br/>- Piece positions<br/>- Game status]
    end

    subgraph "Blueprint UI Layer"  
        MW[WBP_MainWidget<br/>- Piece list container<br/>- Drag source area<br/>- Game UI layout]
        PW[WBP_PieceWidget<br/>- Individual piece UI<br/>- Drag detection<br/>- Visual feedback]
        SW[WBP_StatsWidget<br/>- Time display<br/>- Move counter<br/>- Progress indicator]
        CW[WBP_CompleteWidget<br/>- End game screen<br/>- Final statistics<br/>- Restart option]
    end

    subgraph "Blueprint Gameplay Layer"
        BP_PP[BP_PuzzlePiece<br/>- Visual mesh<br/>- Animation handling<br/>- Audio feedback]
        BP_GM[BP_PuzzleGameMode<br/>- UI integration<br/>- Event handling<br/>- Scene management]
    end

    %% Veri Akışı
    GM --> GS
    PC --> GM
    PP --> GM
    GS --> SW
    
    %% UI Etkileşimleri
    MW --> PW
    PW --> PC
    SW --> MW
    CW --> GM
    
    %% Blueprint-C++ Bağlantıları
    BP_PP --> PP
    BP_GM --> GM
    MW --> BP_GM
    
    %% Oyun Akışı
    PC -.->|"Mouse Events"| PW
    PW -.->|"Drag Start"| PC
    PC -.->|"Spawn Piece"| BP_PP
    BP_PP -.->|"Position Check"| PP
    PP -.->|"Update Stats"| GM
    GM -.->|"Game Complete"| CW

    %% Stil
    classDef cppClass fill:#e1f5fe,stroke:#01579b,stroke-width:2px
    classDef bpClass fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef uiClass fill:#e8f5e8,stroke:#1b5e20,stroke-width:2px
    
    class GM,PC,PP,GS cppClass
    class BP_PP,BP_GM bpClass  
    class MW,PW,SW,CW uiClass


    Harika soru! Projenin durumunu analiz edeyim:
📊 Proje Tamamlanma Durumu
✅ Tamamlanan Kısımlar (~40%)

C++ Core Systems (90% tamamlandı)

✅ PuzzlePiece sınıfı (pozisyon kontrolü, hareket, swap)
✅ PuzzleGameMode (timer, hamle sayacı, completion check)
✅ PuzzlePlayerController (enhanced input, mouse handling)
✅ Basic compilation ve test


Project Setup (100% tamamlandı)

✅ Enhanced Input System kurulumu
✅ Blueprint sınıfları (BP_PuzzlePiece, BP_GameMode, BP_PlayerController)
✅ Input mappings (mouse clicks, keyboard)


Basic UI (30% tamamlandı)

✅ WBP_MainWidget temel yapısı
✅ PlayerController-UI bağlantısı
🔄 WBP_PuzzlePiece (şu an üzerinde çalışıyoruz)



🔄 Devam Eden Kısımlar (~25%)

UI Drag-Drop System (50% progress)

🔄 Puzzle piece listesi
❌ UI'dan 3D sahneye sürükleme
❌ Visual feedback sistemleri


3D Interaction (25% progress)

❌ 3D sahnede piece spawning
❌ 3D sürükle-bırak
❌ Piece swapping animasyonları



❌ Eksik Kısımlar (~35%)

Game UI Complete

❌ WBP_GameStats (timer, move counter görünümü)
❌ WBP_GameComplete (bitiş ekranı)
❌ Random shuffle görselleştirmesi


Polish & Features

❌ Visual feedback (doğru/yanlış pozisyon)
❌ Animation sistemi
❌ Audio feedback (opsiyonel)
❌ Performance optimizasyonu


Final Integration & Testing

❌ Complete gameplay loop test
❌ Bug fixing ve polish
❌ GitHub repo cleanup



🎯 Gerçekçi Tamamlanma Tahmini:
Şu Anki Durum: ~40% tamamlandı
Core sisteminiz sağlam! En zor kısımlar (C++ backend, input system) tamamlandı.
Kalan İş Dağılımı:

UI Drag-Drop (2-3 saat) → +15%
3D Integration (2-3 saat) → +20%
Stats & Complete Screen (1-2 saat) → +15%
Polish & Testing (1-2 saat) → +10%

Toplam kalan süre: 6-10 saat
🚀 İyi Haber:
En kritik ve zor kısımları tamamladınız!

✅ C++ architecture solid
✅ Input system çalışıyor
✅ Basic UI connection var

Kalan kısımlar çoğunlukla "assembly" işi - mevcut sistemleri birbirine bağlamak.
🎯 Öncelik Sırası:
1. (Şu an) UI Drag-Drop → %55
2. 3D Spawning Integration → %75
3. Stats UI → %85
4. Complete Screen → %95
5. Polish → %100
Case study için %85-90 bile çok iyi bir durum! İşveren implementation becerinizi görebilir.
Devam edelim mi? Şu anki momentum'u kaybetmeyelim! 🔥