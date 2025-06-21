#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"

typedef struct Player {
    Camera3D camera;

    Vector3 velocity; 
    bool isMoving;
    bool isSprinting;
    bool isCrouching;
    bool isJumping;

    float baseFOV;
    float sprintFOV;
    float crouchFOV;
    float fovSpeed;// fov transition smoothing

    float baseHeight;
    float crouchHeight;

    float moveSpeed;
    float sprintSpeed;
    float crouchSpeed;
    float acc; // acceleratio for smoothing between states
    float g; // gravity
    float jumpStrength;

    float bobbingTime;
    float bobbingSpeed;
    float bobbingAmount;
    float swayAmount; // makes bobbing a bit more smooth
} Player;


Player InitPlayer();
void UpdatePlayer(Player *player, int cameraMode);

int main() {
    // initialise window
    InitWindow(1920, 1080, "slashcast");
    SetTargetFPS(60);
    SetExitKey(-1); // disables closure if ESC pressed
    SetWindowState(FLAG_FULLSCREEN_MODE);

    // initialise a new player
    Player player = InitPlayer();
    int cameraMode = CAMERA_FIRST_PERSON;
    DisableCursor();

    // initialise binds
    FILE *file = fopen("../config/keybinds.ini", "r");
    /*fscanf(file, "moveForward=%d\n", &keybinds.moveForward);
    repeat for other keys
    fclose(file);*/

    // main game loop
    while (!WindowShouldClose()) {
        UpdatePlayer(&player, cameraMode);
        // render
        BeginDrawing();
            ClearBackground(BLACK);

            // show FPS
            char fps[10];
            sprintf(fps, "%d", GetFPS());
            DrawText(fps ,0 ,0 ,5, DARKGREEN);
            
            // draw map
            BeginMode3D(player.camera);
                DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 32.0f, 32.0f }, LIGHTGRAY);
                // wall
                DrawCube((Vector3){ -15.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, BLUE);
                DrawCube((Vector3){ 15.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, BLUE);
                DrawCube((Vector3){ 0.0f, 2.5f, 15.0f }, 32.0f, 5.0f, 1.0f, BLUE);
                DrawCube((Vector3){ 0.0f, 2.5f, -15.0f}, 32.0f, 5.0f, 1.0f, BLUE);
                // random cube to test hit detection
                DrawCube((Vector3){ 3.0f, 2.5f, 0.0f }, 2.0f, 4.0f, 2.0f, DARKGREEN);
            EndMode3D();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
Player InitPlayer() {
    Player player = { 0 }; // create new player
    // initialise camera on player eyelevel
    player.camera.position = (Vector3){ 0.0f, 2.0f, 4.0f }; // camera position
    player.camera.target = (Vector3){ 0.0f, 2.0f, 0.0f }; // camerea looking at point
    player.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // up vector (rotation towards target)
    player.camera.fovy = 100.0f;
    player.camera.projection = CAMERA_PERSPECTIVE; // camera projection type

    player.baseFOV = 100.0f;
    player.sprintFOV = 110.0f;
    player.crouchFOV = 90.0f;
    player.fovSpeed = 5.0f; // fov transition smoothing

    // bobbing when walking
    player.baseHeight = 2.0f;
    player.bobbingTime = 0.0f;
    player.bobbingSpeed = 10.0f;
    player.bobbingAmount = 0.03f; // change this for bobbing amount customisation
    player.swayAmount = 0.01; // change this for swaying amount customisation

    player.baseHeight = 2.0f;
    player.crouchHeight = 1.0f;

    player.moveSpeed = 4.0f;
    player.sprintSpeed = 7.0f;
    player.crouchSpeed = 2.0f;
    player.acc = 20.0f;

    player.g = 20.0f;
    player.jumpStrength = 6.0f;

    return player;
}
void UpdatePlayer(Player *player, int cameraMode) {
    float dTime = GetFrameTime(); //time in seconds for last frame drawn (delta time)

    // detect movement type (sprinting or crouch walking)
    player->isSprinting = IsKeyDown(KEY_LEFT_SHIFT);
    player->isCrouching = IsKeyDown(KEY_LEFT_CONTROL);

    // detect movement direction
    Vector3 dir = { 0 }; // direction
    if(IsKeyDown(KEY_W)) // dont use else if here because can press two movement keys at once
        dir.z += 1.0f; // forwards
    if(IsKeyDown(KEY_S))
        dir.z -= 1.0f; // backwards
    if(IsKeyDown(KEY_A))
        dir.x -= 1.0f; // left
    if(IsKeyDown(KEY_D))
        dir.x += 1.0f;

    // normalise direction
    float length = sqrtf(dir.x * dir.x + dir.z*dir.z);
    if(length > 0.0f) {
        dir.x /= length;
        dir.z /= length;
    }

    // detect speed and change fov accordingly
    float speed = player->moveSpeed;
    float FOV = player->baseFOV;
    if(player->isSprinting) {
        speed = player->sprintSpeed;
        FOV = player->sprintFOV;
    }
    if(player->isCrouching) {
        speed = player->crouchSpeed;
        FOV = player->crouchFOV;
    }

    // rotate direction by camera yaw assuming camera.target - position is forwards
    Vector3 forward = Vector3Normalize(Vector3Subtract(player->camera.target, player->camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, player->camera.up));
    Vector3 worldDirection = {(right.x * dir.x + forward.x * dir.z) ,
                                0.0f,
                                (right.z * dir.x + forward.z * dir.z)}; // what
    Vector3 targetVelocity = Vector3Scale(worldDirection, speed);

    // make acceleration look nicer
    player->velocity.x += (targetVelocity.x - player->velocity.x) * player->acc * dTime;
    player->velocity.z += (targetVelocity.z - player->velocity.z) * player->acc * dTime;

    // add gravity to movement
    player->velocity.y -= player->g * dTime;

    // jumping
    if(IsKeyPressed(KEY_SPACE) && !(player->isJumping) && !(player->isCrouching)) {
        player->velocity.y = player->jumpStrength;
        player->isJumping = true;
    }

    // ground check
    if(player->camera.position.y <= player->baseHeight && player->velocity.y <= 0) {
        player->isJumping = false;
        player->velocity.y = 0;
    }

    // smooth fov changing when changing movement speeds
    player->camera.fovy += (FOV - player->camera.fovy) * player->fovSpeed * dTime;

    // apply movement to camera
    player->camera.position.x += player->velocity.x * dTime;
    player->camera.position.y += player->velocity.y * dTime;
    player->camera.position.z += player->velocity.z * dTime;

    // apply crouch change to camera y position height
    float crouchY = player->isCrouching ? player->crouchHeight : player->baseHeight;
    if(!(player->isJumping)) {
        player->camera.position.y += (crouchY - player->camera.position.y) * 10.0f * dTime;
    }    

    // detect movement
    player->isMoving = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) ||
                        IsKeyDown(KEY_S) || IsKeyDown(KEY_D);
    UpdateCamera(&player->camera, cameraMode);

    // calculate head bobbing
    float bobOff = 0.0f;
    float swayOff = 0.0f;

    if(player->isMoving) {
        player->bobbingTime += GetFrameTime() * player->bobbingSpeed;
        bobOff = sinf(player->bobbingTime * 2.0f) * player->bobbingAmount;
        swayOff = sinf(player->bobbingTime * 1.0f) * player->swayAmount;
    }
    else {
        player->bobbingTime = 0.0f;
    }
    // apply head bobbing and swaying
    if(!(player->isJumping) && !(player->isCrouching)) {
        player->camera.position.x += swayOff;
        player->camera.position.y += bobOff;
    }

    // update where camera is looking
    Vector2 mouseDelta = GetMouseDelta();
    float sens = 0.003f; // looking sens change this for customisation

    static float yaw = 0.0f, pitch = 0.0f;
    yaw -= mouseDelta.x * sens;
    pitch -= mouseDelta.y * sens;
    pitch = Clamp(pitch, -89.0f*DEG2RAD, 89.0f*DEG2RAD);

    Vector3 dirr = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    // end of sprinting jumping crouching section
    player->camera.target = Vector3Add(player->camera.position, dirr);
}