#include <flecs.h>
#include <raylib.h>
#include <algorithm>
#include <iostream>
#include <cmath>

struct Position
{
    float x;
    float y;
};

struct Velocity
{
    float x;
    float y;
};

struct CircleRenderer
{
    float radius;
    uint8_t r, g, b, a;
};

struct Character
{
    float speed;
    float health;
    float maxHealth;
};

struct Player
{
};

struct Enemy
{
};

struct Bullet
{
    float damage;
    float lifetime;
};

struct Input
{
    float x;
    float y;
};

struct GameState
{
    bool gameOver;
    bool gameWon;
    bool started;
};

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);

    InitWindow(1920, 1080, "game");

    ToggleBorderlessWindowed();

    SetTargetFPS(60);

    flecs::world world{};

    world.component<Position>()
        .member<float>("x")
        .member<float>("y");

    world.component<Velocity>()
        .member<float>("x")
        .member<float>("y");

    world.component<CircleRenderer>()
        .member<float>("radius")
        .member<uint8_t>("r")
        .member<uint8_t>("g")
        .member<uint8_t>("b")
        .member<uint8_t>("a");

    world.component<Character>()
        .member<float>("speed")
        .member<float>("health")
        .member<float>("maxHealth");

    world.component<Bullet>()
        .member<float>("damage")
        .member<float>("lifetime");

    world.component<Input>()
        .add(flecs::Singleton);

    world.set<Input>({0.f, 0.f});

    world.set<GameState>({false, false, false});

    world.system<Input>()
        .kind(flecs::PreUpdate)
        .each([](Input &input)
              {
            float dx =
                float(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) -
                float(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));

            float dy =
                float(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) -
                float(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP));

            float len2 = dx * dx + dy * dy;

            if (len2 > 0.f)
            {
                float invLen = 1.f / std::sqrt(len2);
                dx *= invLen;
                dy *= invLen;
            }

            input = {dx, dy}; });

    world.system<Velocity, const Character, const Input>()
        .with<const Player>()
        .kind(flecs::OnUpdate)
        .each([](Velocity &velocity,
                 const Character &character,
                 const Input &input)
              { velocity = {
                    input.x * character.speed,
                    input.y * character.speed}; });

    auto player_query = world.query<const Position, const Player>();
    auto enemy_query = world.query<const Position, const Enemy>();

    world.system<Velocity, const Character, const Position>()
        .with<const Enemy>()
        .kind(flecs::OnUpdate)
        .each([player_query, enemy_query](
                  flecs::entity self,
                  Velocity &velocity,
                  const Character &character,
                  const Position &position)
              {
            Position player{};

            player_query.each([&](const Position &p, const Player &)
            {
                player = p;
            });

            float steerX = player.x - position.x;
            float steerY = player.y - position.y;

            float len = std::sqrt(steerX * steerX + steerY * steerY);

            if (len > 0.001f)
            {
                steerX /= len;
                steerY /= len;
            }

            constexpr float separationRadius = 26.f;

            enemy_query.each([&](flecs::entity other,
                                 const Position &otherPos,
                                 const Enemy &)
            {
                if (other == self)
                    return;

                float dx = position.x - otherPos.x;
                float dy = position.y - otherPos.y;

                float dist2 = dx * dx + dy * dy;

                if (dist2 > separationRadius * separationRadius ||
                    dist2 < 0.0001f)
                    return;

                float dist = std::sqrt(dist2);

                float force = (separationRadius - dist) / separationRadius;

                steerX += (dx / dist) * force * 2.5f;
                steerY += (dy / dist) * force * 2.5f;
            });

            len = std::sqrt(steerX * steerX + steerY * steerY);

            if (len > 0.001f)
            {
                steerX /= len;
                steerY /= len;
            }

            Velocity target{
                steerX * character.speed,
                steerY * character.speed
            };

            constexpr float smoothing = 0.12f;

            velocity.x += (target.x - velocity.x) * smoothing;
            velocity.y += (target.y - velocity.y) * smoothing;

            if (std::sqrt(
                (player.x - position.x) * (player.x - position.x) +
                (player.y - position.y) * (player.y - position.y)) < 10.f)
            {
                velocity = {0.f, 0.f};
            } });

    world.system<const Position, Character>()
        .with<Player>()
        .kind(flecs::OnUpdate)
        .each([&](flecs::entity player,
                  const Position &playerPos,
                  Character &character)
              {
    auto enemies = world.query<const Position, const Enemy>();

    enemies.each([&](flecs::entity enemy,
                     const Position &enemyPos,
                     const Enemy &)
    {
        float dx = playerPos.x - enemyPos.x;
        float dy = playerPos.y - enemyPos.y;

        float dist2 = dx * dx + dy * dy;

        constexpr float collisionRadius = 30.f;

        if (dist2 < collisionRadius * collisionRadius)
        {
            character.health -= 25.f * world.delta_time();

            if (character.health <= 0)
            {
                world.set<GameState>({true});
            }
        }
    }); });

    world.system<Position, const Velocity>()
        .kind(flecs::OnUpdate)
        .run([](flecs::iter &it)
             {
            float dt = it.delta_time();

            while (it.next())
            {
                auto positions = it.field<Position>(0);
                auto velocities = it.field<const Velocity>(1);

                for (auto i : it)
                {
                    positions[i].x += velocities[i].x * dt;
                    positions[i].y += velocities[i].y * dt;
                }
            } });

    world.system<>()
        .kind(flecs::PostUpdate)
        .run([&](flecs::iter &it)
             {
        auto state = &world.get_mut<GameState>();

        if (!state->started || state->gameOver || state->gameWon)
            return;

        bool hasEnemies = false;

        world.query<const Enemy>().each([&](flecs::entity, const Enemy&)
        {
            hasEnemies = true;
        });

        if (!hasEnemies)
        {
            state->gameWon = true;
        } });

    world.system<Position, const CircleRenderer>()
        .without<Bullet>()
        .kind(flecs::PostUpdate)
        .each([](Position &position, const CircleRenderer &circle)
              {
            position.x = std::clamp(
                position.x,
                circle.radius,
                static_cast<float>(GetScreenWidth()) - circle.radius);

            position.y = std::clamp(
                position.y,
                circle.radius,
                static_cast<float>(GetScreenHeight()) - circle.radius); });

    world.system<const Position, const CircleRenderer>()
        .kind(flecs::PostUpdate)
        .each([](const Position &position, const CircleRenderer &circle)
              { DrawCircleV(
                    Vector2{position.x, position.y},
                    circle.radius,
                    Color{circle.r, circle.g, circle.b, circle.a}); });

    world.system<const Position>()
        .kind(flecs::PostUpdate)
        .each([](flecs::entity player, const Position &position)
              {
            if (!player.has<Player>())
                return;

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                Vector2 mouse = GetMousePosition();

                float dx = mouse.x - position.x;
                float dy = mouse.y - position.y;

                float length = std::sqrt(dx * dx + dy * dy);

                if (length > 0.001f)
                {
                    dx /= length;
                    dy /= length;
                }

                constexpr float bulletSpeed = 600.f;

                flecs::entity bullet = player.world()
                    .entity()
                    .is_a(player.world().lookup("BulletPrefab"));

                bullet.set<Position>({
                    position.x,
                    position.y
                });

                bullet.set<Velocity>({
                    dx * bulletSpeed,
                    dy * bulletSpeed
                });
            } });

    world.system<const Position, const Bullet>()
        .kind(flecs::OnUpdate)
        .each([](flecs::entity bullet,
                 const Position &position,
                 const Bullet &)
              {
            constexpr float padding = 50.f;

            if (position.x < -padding ||
                position.x > GetScreenWidth() + padding ||
                position.y < -padding ||
                position.y > GetScreenHeight() + padding)
            {
                bullet.destruct();
            } });

    auto enemies = world.query<const Position, const Enemy, Character>();

    world.system<const Position, const Bullet>()
        .kind(flecs::PostUpdate)
        .each([enemies](flecs::entity bullet,
                        const Position &bp,
                        const Bullet &bulletData)
              { enemies.each([&](flecs::entity enemy,
                                 const Position &ep,
                                 const Enemy &,
                                 Character &character)
                             {
        float dx = bp.x - ep.x;
        float dy = bp.y - ep.y;

        float dist2 = dx * dx + dy * dy;

        constexpr float hitRadius = 20.f;

        if (dist2 < hitRadius * hitRadius)
        {
            character.health -= bulletData.damage;

            bullet.destruct();

            if (character.health <= 0)
            {
                enemy.destruct();
            }
        } }); });

    world.system<Bullet>()
        .kind(flecs::OnUpdate)
        .each([](flecs::entity bullet, Bullet &data)
              {
    data.lifetime -= GetFrameTime();

    if (data.lifetime <= 0)
    {
        bullet.destruct();
    } });

    world.system<const Position, const Character, const CircleRenderer>()
        .kind(flecs::PostUpdate)
        .each([](const Position &pos,
                 const Character &character,
                 const CircleRenderer &circle)
              {
    float width = circle.radius * 2.f;

    float ratio = character.health / character.maxHealth;

    DrawRectangle(
        pos.x - width / 2,
        pos.y - circle.radius - 10,
        width,
        5,
        GRAY);

    DrawRectangle(
        pos.x - width / 2,
        pos.y - circle.radius - 10,
        width * ratio,
        5,
        GREEN); });

#ifdef __INTELLISENSE__
    const char flecs_script[] = {0};
#else
    const char flecs_script[] = {
#embed "assets/game.flecs"
    };
#endif

    world.script_run("game.flecs", flecs_script);

    world.get_mut<GameState>().started = true;

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        auto state = &world.get<GameState>();

        if (!state->gameOver && !state->gameWon)
        {
            world.progress(GetFrameTime());
        }
        else if (state->gameOver)
        {
            DrawText(
                "GAME OVER",
                GetScreenWidth() / 2 - 100,
                GetScreenHeight() / 2 - 20,
                40,
                RED);
        }
        else if (state->gameWon)
        {
            DrawText(
                "YOU WIN",
                GetScreenWidth() / 2 - 90,
                GetScreenHeight() / 2 - 20,
                40,
                GREEN);
        }

        DrawFPS(10, 10);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
