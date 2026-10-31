// Порт старого проекта с TXLib на C++ + SFML 2.x
// Логика та же: меню, настройки (RU/EN), город с параллаксом, 2-я локация со стеной,
// лут машины и вагона, инвентарь. Текстуры берутся из ./textures как раньше.
//
// Файл в UTF-8. Для MSVC в CMake включён флаг /utf-8.

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kW = 1080.f;
constexpr float kH = 720.f;
constexpr float kGroundY = 550.f;     // координата Y игрока на 1-й локации
constexpr float kPlayer = 64.f;       // размер спрайта игрока
constexpr float kWalkSpeed = 300.f;   // px/сек
constexpr float kBgSpeed = 60.f;      // скорость параллакса при ходьбе (px/сек)
constexpr float kSkySpeed = 40.f;     // скорость неба (px/сек)
constexpr float kSlot = 90.f;
constexpr int kSlotCount = 15;
constexpr float kCarBaseX = 250.f;    // где на фоне стоит машина

enum class Lang { Ru, En };

enum class K {
    Start, Settings, About, Exit, Back, Language,
    PauseTitle, Continue, MainMenu,
    AboutTitle, AboutBody,
    HintWall, WallBroken, WallHp,
    Inventory, Car, Train, MissingTextures
};

const char* tr(Lang lang, K key) {
    static const char* const ru[] = {
        "Начать игру", "Настройки", "Об авторе", "Выход", "Вернуться", "Язык: ",
        "Игра приостановлена", "Продолжить", "Главное меню",
        "Об авторе:",
        "Честно говоря, не знаю, что тут написать,\nно связаться со мной можно в телеграме: @Manfy4",
        "Сломайте стену (ЛКМ по ней)", "Готово! Теперь можно обыскать вагон", "Прочность стены: ",
        "Инвентарь", "Машина", "Вагон", "Не найдено текстур: "};
    static const char* const en[] = {
        "Start", "Settings", "About me", "Exit", "Back", "Language: ",
        "Game is paused", "Return", "Main menu",
        "About me:",
        "To be honest, I don't know what to write here,\nbut you can contact me on Telegram: @Manfy4",
        "Break the wall (LMB on it)", "Done! Now you can search the train car", "Wall health: ",
        "Inventory", "Car", "Train car", "Missing textures: "};
    return (lang == Lang::Ru ? ru : en)[static_cast<int>(key)];
}

enum Item { kWater = 1, kMed = 2, kPistol = 3, kAmmo = 4 };

sf::FloatRect invSlotRect(int i) {
    return {50.f + 100.f * (i % 5), 30.f + 100.f * (i / 5), kSlot, kSlot};
}
sf::FloatRect lootSlotRect(int i) {
    return {550.f + 100.f * (i % 5), 30.f + 100.f * (i / 5), kSlot, kSlot};
}
sf::FloatRect menuBtn(int i) { return {20.f, 300.f + 50.f * i, 220.f, 40.f}; }
sf::FloatRect backBtn() { return {20.f, 670.f, 170.f, 40.f}; }
sf::FloatRect langBtn() { return {20.f, 50.f, 300.f, 40.f}; }
sf::FloatRect pauseBtn(int i) { return {440.f, 290.f + 70.f * i, 200.f, 50.f}; }

// Невидимая "стена" для ног игрока: нельзя стоять выше линии feetLimit на отрезке [x1, x2]
sf::FloatRect post(float x1, float x2, float feetLimit) {
    return {x1, 444.f, x2 - x1, feetLimit - 450.f};
}

// Препятствия 2-й локации (числа взяты из старого кода, при необходимости подправь)
const std::vector<sf::FloatRect> kBlockers = {
    post(30, 160, 555), post(150, 260, 520), post(215, 280, 484),   // левые завалы
    post(800, 880, 490), post(820, 930, 510), post(920, 1045, 560), // правые завалы
    post(490, 550, 595),                                            // столб посередине
};
const sf::FloatRect kWallBlocker = post(273, 816, 535);            // стена (пока цела)

const sf::FloatRect kWallArea(260, 450, 560, 85);   // куда бить, чтобы сломать стену
const sf::FloatRect kTrainArea(235, 205, 535, 260); // куда кликать, когда стена сломана

struct Assets {
    sf::Font font;
    bool fontOk = false;
    sf::Texture logo, menuBg, sky, housesTrees, houses, cars, road, loc2;
    sf::Texture idle[4], idleL[4], walk[6], walkL[6];
    sf::Texture water, med, pistol, ammo;
    std::vector<std::string> missing;

    // Белый цвет считаем прозрачным, как раньше в TransparentBlt(..., TX_WHITE)
    bool tex(sf::Texture& t, const std::string& path) {
        sf::Image img;
        if (!img.loadFromFile(path)) {
            missing.push_back(path);
            return false;
        }
        img.createMaskFromColor(sf::Color::White);
        return t.loadFromImage(img);
    }

    void load() {
        const char* fonts[] = {
            "assets/font.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
        };
        for (const char* f : fonts) {
            if (font.loadFromFile(f)) {
                fontOk = true;
                break;
            }
        }
        if (!fontOk) std::cerr << "Не найден шрифт: положи любой .ttf в assets/font.ttf\n";

        tex(logo, "textures/logo/logo.bmp");
        tex(menuBg, "textures/bg/mainbg.bmp");
        tex(sky, "textures/bg/city/sky.bmp");
        tex(housesTrees, "textures/bg/city/houses&trees_bg.bmp");
        tex(houses, "textures/bg/city/houses.bmp");
        tex(cars, "textures/bg/city/car_trees.bmp");
        tex(road, "textures/bg/city/road.bmp");
        tex(loc2, "textures/bg/railway/mainbg_loc2.png");

        for (int i = 0; i < 4; ++i) {
            std::string n = std::to_string(i + 1);
            tex(idle[i], "textures/persons/Idle/" + n + ".bmp");
            tex(idleL[i], "textures/persons/Idle/L" + n + ".bmp");
        }
        for (int i = 0; i < 6; ++i) {
            std::string n = std::to_string(i + 1);
            tex(walk[i], "textures/persons/Walk/" + n + ".bmp");
            tex(walkL[i], "textures/persons/Walk/L" + n + ".bmp");
        }

        tex(med, "textures/items/heal_texture.bmp");
        tex(water, "textures/items/water_texture.bmp");
        tex(pistol, "textures/weapon/Pistol.bmp");
        tex(ammo, "textures/ammo/PistolAmmo.bmp");

        for (const auto& m : missing) std::cerr << "Не найдена текстура: " << m << "\n";
    }

    const sf::Texture& item(int type) const {
        switch (type) {
            case kWater: return water;
            case kMed: return med;
            case kPistol: return pistol;
            default: return ammo;
        }
    }
};

struct Inventory {
    std::map<int, int> count;   // предмет -> количество
    std::map<int, int> slotOf;  // предмет -> номер слота

    void add(int item) {
        if (count[item] == 0) {
            for (int s = 0; s < kSlotCount; ++s) {
                bool used = false;
                for (const auto& kv : slotOf) {
                    if (kv.second == s) used = true;
                }
                if (!used) {
                    slotOf[item] = s;
                    break;
                }
            }
        }
        ++count[item];
    }
};

// Контейнер с лутом (машина или вагон): один случайный предмет в случайной ячейке
struct Container {
    K title = K::Car;
    std::vector<int> bonus;   // что дополнительно выдаётся вместе с предметом
    bool rolled = false;
    bool looted = false;
    int item = 0;
    int slot = 0;

    void roll(std::mt19937& rng) {
        item = static_cast<int>(rng() % 2) + 1;  // вода или аптечка
        slot = static_cast<int>(rng() % kSlotCount);
        rolled = true;
    }
};

enum class Screen { Logo, Menu, About, Settings, Playing };

class Game {
public:
    Game()
        : win(sf::VideoMode(static_cast<unsigned>(kW), static_cast<unsigned>(kH)),
              "Survival 2D (alpha 0.1)", sf::Style::Titlebar | sf::Style::Close) {
        win.setFramerateLimit(60);
        A.load();
        car.title = K::Car;
        car.bonus = {kPistol};
        train.title = K::Train;
        train.bonus = {kAmmo};
    }

    void run() {
        sf::Clock clock;
        while (win.isOpen()) {
            sf::Event e;
            while (win.pollEvent(e)) handleEvent(e);
            float dt = std::min(clock.restart().asSeconds(), 0.05f);
            update(dt);
            draw();
        }
    }

private:
    sf::RenderWindow win;
    Assets A;
    std::mt19937 rng{std::random_device{}()};

    Lang lang = Lang::Ru;
    Screen screen = Screen::Logo;
    bool paused = false;
    bool settingsFromPause = false;
    float logoTimer = 0.f;

    // игрок
    float px = 1.f, py = kGroundY;
    bool facingLeft = false, moving = false;
    float animTime = 0.f;
    int location = 1;

    // мир
    float bgScroll = 0.f, skyScroll = 0.f;
    int wallHp = 10;
    bool trainHintShown = false;

    // инвентарь
    bool invOpen = false;
    Inventory inv;
    Container car, train;
    Container* active = nullptr;

    // ---------- вспомогательные ----------
    sf::Vector2f mousePos() const { return sf::Vector2f(sf::Mouse::getPosition(win)); }

    static sf::String U(const std::string& s) { return sf::String::fromUtf8(s.begin(), s.end()); }

    void text(const std::string& s, float x, float y, unsigned size,
              sf::Color c = sf::Color::White, bool outline = false) {
        if (!A.fontOk) return;
        sf::Text t(U(s), A.font, size);
        t.setFillColor(c);
        if (outline) {
            t.setOutlineColor(sf::Color::Black);
            t.setOutlineThickness(2.f);
        }
        t.setPosition(x, y);
        win.draw(t);
    }

    void textCentered(const std::string& s, const sf::FloatRect& r, unsigned size, sf::Color c) {
        if (!A.fontOk) return;
        sf::Text t(U(s), A.font, size);
        t.setFillColor(c);
        sf::FloatRect b = t.getLocalBounds();
        t.setPosition(std::floor(r.left + (r.width - b.width) / 2 - b.left),
                      std::floor(r.top + (r.height - b.height) / 2 - b.top));
        win.draw(t);
    }

    void rect(const sf::FloatRect& r, sf::Color fill, sf::Color outline = sf::Color::Transparent,
              float thickness = 0.f) {
        sf::RectangleShape s({r.width, r.height});
        s.setPosition(r.left, r.top);
        s.setFillColor(fill);
        s.setOutlineColor(outline);
        s.setOutlineThickness(thickness);
        win.draw(s);
    }

    void button(const sf::FloatRect& r, const std::string& label) {
        bool hover = r.contains(mousePos());
        rect(r, hover ? sf::Color(210, 210, 210) : sf::Color::White);
        textCentered(label, r, 26, sf::Color::Black);
    }

    void blit(const sf::Texture& t, float x, float y, float w, float h) {
        sf::Vector2u s = t.getSize();
        if (!s.x || !s.y) return;
        sf::Sprite sp(t);
        sp.setPosition(x, y);
        sp.setScale(w / s.x, h / s.y);
        win.draw(sp);
    }

    void drawItem(int item, const sf::FloatRect& slot) {
        const sf::Texture& t = A.item(item);
        sf::Vector2u s = t.getSize();
        if (!s.x || !s.y) return;
        float sc = std::min((kSlot - 10.f) / s.x, (kSlot - 10.f) / s.y);
        float w = s.x * sc, h = s.y * sc;
        blit(t, slot.left + (kSlot - w) / 2, slot.top + (kSlot - h) / 2, w, h);
    }

    // ---------- состояние игры ----------
    void openContainer(Container& c) {
        if (!c.looted && !c.rolled) c.roll(rng);
        active = &c;
        invOpen = true;
    }

    void closeInventory() {
        invOpen = false;
        active = nullptr;
    }

    bool blocked(float x, float y) const {
        sf::FloatRect feet(x + 16.f, y + kPlayer - 6.f, 32.f, 6.f);
        for (const auto& b : kBlockers) {
            if (feet.intersects(b)) return true;
        }
        return wallHp > 0 && feet.intersects(kWallBlocker);
    }

    bool nearCar(sf::Vector2f m) const {
        if (location != 1) return false;
        float o = std::fmod(bgScroll, kW);
        if (o < 0) o += kW;
        for (float off : {-kW, 0.f}) {
            float cx = kCarBaseX + o + off;
            if (m.x >= cx && m.x <= cx + 380.f && px < cx + 420.f && px + kPlayer > cx - 40.f)
                return true;
        }
        return false;
    }

    // ---------- события ----------
    void handleEvent(const sf::Event& e) {
        if (e.type == sf::Event::Closed) {
            win.close();
        } else if (e.type == sf::Event::KeyPressed) {
            onKey(e.key.code);
        } else if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            onClick(sf::Vector2f(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y)));
        }
    }

    void goBackFromSettings() {
        screen = settingsFromPause ? Screen::Playing : Screen::Menu;
    }

    void onKey(sf::Keyboard::Key k) {
        if (screen == Screen::Logo) {
            screen = Screen::Menu;
        } else if (screen == Screen::About) {
            if (k == sf::Keyboard::Escape) screen = Screen::Menu;
        } else if (screen == Screen::Settings) {
            if (k == sf::Keyboard::Escape) goBackFromSettings();
        } else if (screen == Screen::Playing) {
            if (k == sf::Keyboard::Escape) {
                if (invOpen) closeInventory();
                else paused = !paused;
            } else if (k == sf::Keyboard::E && !paused) {
                if (invOpen) closeInventory();
                else {
                    invOpen = true;
                    active = nullptr;
                }
            }
        }
    }

    void onClick(sf::Vector2f m) {
        switch (screen) {
            case Screen::Logo:
                screen = Screen::Menu;
                break;
            case Screen::Menu:
                if (menuBtn(0).contains(m)) { screen = Screen::Playing; paused = false; }
                else if (menuBtn(1).contains(m)) { screen = Screen::Settings; settingsFromPause = false; }
                else if (menuBtn(2).contains(m)) { screen = Screen::About; }
                else if (menuBtn(3).contains(m)) { win.close(); }
                break;
            case Screen::About:
                if (backBtn().contains(m)) screen = Screen::Menu;
                break;
            case Screen::Settings:
                if (langBtn().contains(m)) lang = (lang == Lang::Ru) ? Lang::En : Lang::Ru;
                else if (backBtn().contains(m)) goBackFromSettings();
                break;
            case Screen::Playing:
                onGameClick(m);
                break;
        }
    }

    void onGameClick(sf::Vector2f m) {
        if (paused) {
            if (pauseBtn(0).contains(m)) paused = false;
            else if (pauseBtn(1).contains(m)) { screen = Screen::Settings; settingsFromPause = true; }
            else if (pauseBtn(2).contains(m)) { paused = false; screen = Screen::Menu; }
            return;
        }
        if (invOpen) {
            if (active && !active->looted && lootSlotRect(active->slot).contains(m)) {
                inv.add(active->item);
                for (int b : active->bonus) inv.add(b);
                active->looted = true;
            }
            return;
        }
        if (nearCar(m)) {
            openContainer(car);
        } else if (location == 2) {
            if (wallHp > 0) {
                if (kWallArea.contains(m)) --wallHp;
            } else if (kTrainArea.contains(m)) {
                openContainer(train);
                trainHintShown = true;
            }
        }
    }

    // ---------- обновление ----------
    void update(float dt) {
        if (screen == Screen::Logo) {
            logoTimer += dt;
            if (logoTimer > 1.5f) screen = Screen::Menu;
            return;
        }
        if (screen != Screen::Playing || paused || invOpen) return;

        animTime += dt;
        if (location == 1) skyScroll += kSkySpeed * dt;
        updatePlayer(dt);
    }

    void updatePlayer(float dt) {
        using sf::Keyboard;
        float dx = (Keyboard::isKeyPressed(Keyboard::D) ? 1.f : 0.f) - (Keyboard::isKeyPressed(Keyboard::A) ? 1.f : 0.f);
        float dy = 0.f;
        if (location == 2)
            dy = (Keyboard::isKeyPressed(Keyboard::S) ? 1.f : 0.f) - (Keyboard::isKeyPressed(Keyboard::W) ? 1.f : 0.f);

        moving = (dx != 0.f || dy != 0.f);
        if (dx < 0) facingLeft = true;
        else if (dx > 0) facingLeft = false;

        float oldX = px;
        float nx = px + dx * kWalkSpeed * dt;

        if (location == 1) {
            nx = std::max(nx, 0.f);
            py = kGroundY;
            px = nx;
            if (px > kW) {            // ушли за правый край -> 2-я локация
                location = 2;
                px = -kPlayer;
            }
            bgScroll -= (px - oldX) * (kBgSpeed / kWalkSpeed);
        } else {
            nx = std::clamp(nx, -kPlayer, kW - kPlayer);
            if (!blocked(nx, py)) px = nx;
            float ny = std::clamp(py + dy * kWalkSpeed * dt, 450.f - kPlayer, kGroundY);
            if (!blocked(px, ny)) py = ny;
        }
    }

    // ---------- отрисовка ----------
    void draw() {
        win.clear(sf::Color::Black);
        switch (screen) {
            case Screen::Logo: drawLogo(); break;
            case Screen::Menu: drawMenu(); break;
            case Screen::About: drawAbout(); break;
            case Screen::Settings: drawSettings(); break;
            case Screen::Playing: drawGame(); break;
        }
        win.display();
    }

    void drawLogo() {
        win.clear(sf::Color::White);
        blit(A.logo, 415, 250, 256, 256);
        textCentered("alpha 0.1", sf::FloatRect(0, 560, kW, 24), 20, sf::Color::Black);
    }

    void drawMenu() {
        blit(A.menuBg, 0, 0, kW, kH);
        button(menuBtn(0), tr(lang, K::Start));
        button(menuBtn(1), tr(lang, K::Settings));
        button(menuBtn(2), tr(lang, K::About));
        button(menuBtn(3), tr(lang, K::Exit));
        if (!A.missing.empty()) {
            text(std::string(tr(lang, K::MissingTextures)) + std::to_string(A.missing.size()) + " (см. консоль / see console)",
                 20, 690, 18, sf::Color(255, 90, 90), true);
        }
    }

    void drawAbout() {
        win.clear(sf::Color(25, 25, 30));
        text(tr(lang, K::AboutTitle), 20, 20, 34);
        text(tr(lang, K::AboutBody), 20, 80, 24);
        button(backBtn(), tr(lang, K::Back));
    }

    void drawSettings() {
        win.clear(sf::Color::Black);
        button(langBtn(), std::string(tr(lang, K::Language)) + (lang == Lang::Ru ? "Русский" : "English"));
        button(backBtn(), tr(lang, K::Back));
    }

    void drawGame() {
        if (invOpen) {
            drawInventoryScreen();
            return;
        }
        drawWorld();
        drawPlayer();
        drawLocationOverlay();
        if (paused) drawPause();
    }

    void drawWorld() {
        if (location == 1) {
            float s = std::fmod(skyScroll, kW);
            blit(A.sky, s, 0, kW, kH);
            blit(A.sky, s - kW, 0, kW, kH);

            float o = std::fmod(bgScroll, kW);
            if (o < 0) o += kW;
            for (const sf::Texture* layer : {&A.housesTrees, &A.houses, &A.cars, &A.road}) {
                blit(*layer, o, 0, kW, kH);
                blit(*layer, o - kW, 0, kW, kH);
            }
        } else {
            blit(A.loc2, 0, 0, kW, kH);
        }
    }

    void drawPlayer() {
        const sf::Texture* t;
        if (moving) {
            int i = static_cast<int>(animTime / 0.1f) % 6;
            t = facingLeft ? &A.walkL[i] : &A.walk[i];
        } else {
            int i = static_cast<int>(animTime / 0.15f) % 4;
            t = facingLeft ? &A.idleL[i] : &A.idle[i];
        }
        blit(*t, px, py, kPlayer, kPlayer);
    }

    void banner(const std::string& s, float x) {
        sf::FloatRect r(x, 0, kW - x, 50);
        rect(r, sf::Color::Black);
        textCentered(s, r, 24, sf::Color::White);
    }

    void drawLocationOverlay() {
        if (location != 2) return;
        if (wallHp > 0) {
            banner(tr(lang, K::HintWall), 730);
            sf::Vector2f m = mousePos();
            if (kWallArea.contains(m)) {
                rect({278, 535, 207, 2}, sf::Color::White);
                rect({568, 535, 252, 2}, sf::Color::White);
                text(std::string(tr(lang, K::WallHp)) + std::to_string(wallHp), m.x - 30, m.y + 15, 20,
                     sf::Color::White, true);
            }
        } else if (!trainHintShown) {
            banner(tr(lang, K::WallBroken), 630);
        }
    }

    void drawPause() {
        rect({300, 200, 480, 300}, sf::Color::Black, sf::Color::White, 2.f);
        textCentered(tr(lang, K::PauseTitle), sf::FloatRect(300, 215, 480, 50), 34, sf::Color::White);
        button(pauseBtn(0), tr(lang, K::Continue));
        button(pauseBtn(1), tr(lang, K::Settings));
        button(pauseBtn(2), tr(lang, K::MainMenu));
    }

    void slotBox(const sf::FloatRect& r) {
        rect(r, sf::Color(45, 45, 55), sf::Color(110, 110, 120), 2.f);
    }

    void drawInventoryScreen() {
        win.clear(sf::Color(15, 15, 20));
        text(tr(lang, K::Inventory), 50, 2, 22);
        for (int i = 0; i < kSlotCount; ++i) slotBox(invSlotRect(i));
        for (const auto& kv : inv.slotOf) {
            sf::FloatRect r = invSlotRect(kv.second);
            drawItem(kv.first, r);
            text(std::to_string(inv.count[kv.first]), r.left + kSlot - 28, r.top + kSlot - 26, 20);
        }
        if (active) {
            text(tr(lang, active->title), 550, 2, 22);
            for (int i = 0; i < kSlotCount; ++i) slotBox(lootSlotRect(i));
            if (!active->looted) drawItem(active->item, lootSlotRect(active->slot));
        }
    }
};

}  // namespace

int main() {
    Game game;
    game.run();
    return 0;
}
