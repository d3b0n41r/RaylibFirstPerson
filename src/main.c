#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define LORENDER_WIDTH 640
#define LORENDER_HEIGHT 480
#define HIRENDER_WIDTH 1920
#define HIRENDER_HEIGHT 1080

typedef struct Player {
    Camera3D camera;
    BoundingBox playerBox;

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

typedef struct {
    bool collided;
    Vector3 normal;
} CollisionResult;

#define NUM_BOXES 256
BoundingBox wallBoxes[NUM_BOXES];

Player InitPlayer();
void UpdatePlayer(Player *player, int cameraMode);
CollisionResult CheckCameraCollision(Camera camera, Vector3 velocity);

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

    // initialise base model
    Model character = LoadModel("../assets/basic_man.glb");
    // Texture2D texture = LoadTexture("assets/character_texture.png");
    // character.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

    // shaders
    Shader posterization = LoadShader(0, TextFormat("../assets/shaders/posterization.fs", 330));


    // skybox stuff
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Model skybox = LoadModelFromMesh(cube);
    skybox.materials[0].shader = LoadShader(TextFormat("../assets/shaders/skybox.vs", 330),
                                            TextFormat("../assets/shaders/skybox.fs", 330));
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "environmentMap"), (int[1]){ MATERIAL_MAP_CUBEMAP }, SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "doGamma"), (int[1]) { 0 }, SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "vflipped"), (int[1]){ 0 }, SHADER_UNIFORM_INT);
    Shader shdrCubemap = LoadShader(TextFormat("../assets/shaders/cubemap.vs", 330),
                                    TextFormat("../assets/shaders/cubemap.fs", 330));
    SetShaderValue(shdrCubemap, GetShaderLocation(shdrCubemap, "equirectangularMap"), (int[1]){ 0 }, SHADER_UNIFORM_INT);
    Image img = LoadImage("../assets/cubemaps/Cubemap_Sky_21-512x512.png");
    skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);    // CUBEMAP_LAYOUT_PANORAMA
    UnloadImage(img);

    // initialise binds
    FILE *file = fopen("../config/keybinds.ini", "r");
    /*fscanf(file, "moveForward=%d\n", &keybinds.moveForward);
    repeat for other keys
    fclose(file);*/
    wallBoxes[0] = (BoundingBox){  // Left wall
        .min = (Vector3){ -16.0f, 0.0f, -16.0f },
        .max = (Vector3){ -15.0f, 5.0f, 16.0f }
    };
    wallBoxes[1] = (BoundingBox){  // Right wall
        .min = (Vector3){ 15.0f, 0.0f, -16.0f },
        .max = (Vector3){ 16.0f, 5.0f, 16.0f }
    };
    wallBoxes[2] = (BoundingBox){  // Front wall
        .min = (Vector3){ -16.0f, 0.0f, 15.0f },
        .max = (Vector3){ 16.0f, 5.0f, 16.0f }
    };
    /*wallBoxes[3] = (BoundingBox){  // Back wall
        .min = (Vector3){ -16.0f, 0.0f, -16.0f },
        .max = (Vector3){ 16.0f, 5.0f, -15.0f }
    };*/
    wallBoxes[4] = (BoundingBox){
        .min = (Vector3){ 2.0f, 0.0f, -1.0f },
        .max = (Vector3){ 4.0f, 4.0f, 1.0f }
    };
    RenderTexture2D renderTarget = LoadRenderTexture(LORENDER_WIDTH, LORENDER_HEIGHT);
    // main game loop
    while (!WindowShouldClose()) {
        UpdatePlayer(&player, cameraMode);
        BeginTextureMode(renderTarget);
            ClearBackground(BLACK);
            // draw map
            BeginMode3D(player.camera);
                // skybox
                rlDisableBackfaceCulling();
                rlDisableDepthTest();
                DrawModel(skybox, (Vector3){0, 0, 0}, 1.0f, WHITE);
                rlEnableDepthTest();
                rlEnableBackfaceCulling();

                // Draw axis lines from origin
                DrawLine3D((Vector3){0, 0, 0}, (Vector3){1, 0, 0}, RED);     // X-axis (right)
                DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 1, 0}, GREEN);   // Y-axis (up)
                DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, 1}, BLUE);    // Z-axis (forward)


                DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 32.0f, 32.0f }, LIGHTGRAY);
                // wall
                DrawCube((Vector3){ -15.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, BLUE);
                DrawCube((Vector3){ 15.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, BLUE);
                DrawCube((Vector3){ 0.0f, 2.5f, 15.0f }, 32.0f, 5.0f, 1.0f, BLUE);
                //DrawCube((Vector3){ 0.0f, 2.5f, -15.0f}, 32.0f, 5.0f, 1.0f, BLUE);
                // random cube to test hit detection
                DrawCube((Vector3){ 3.0f, 2.5f, 0.0f }, 2.0f, 4.0f, 2.0f, DARKGREEN);

                // draw player
                Vector3 bodyPos = player.camera.position;
                bodyPos.y -= 0.75f;
                //get camera forward vector
                Vector3 forwardCam = Vector3Normalize((Vector3){
                    player.camera.target.x - player.camera.position.x,
                    0,
                    player.camera.target.z - player.camera.position.z
                });
                bodyPos = Vector3Add(bodyPos, Vector3Scale(forwardCam, 0.0f));
                float camYaw = atan2f(forwardCam.x, forwardCam.z) * RAD2DEG;
                DrawModelEx(character, bodyPos, (Vector3){ 0.0f, 5.0f, 0.0f}, camYaw, (Vector3){ 0.45f, 0.45f, 0.45f }, GREEN);
            EndMode3D();
        EndTextureMode();
        // render
        BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(posterization);
                DrawTexturePro(
                        renderTarget.texture,
                        (Rectangle){ 0, 0, (float)LORENDER_WIDTH, -(float)LORENDER_HEIGHT },
                        (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                        (Vector2){ 0, 0 },
                        0.0f,
                        WHITE
                );
            EndShaderMode();

            // show FPS
            char fps[10];
            sprintf(fps, "%d", GetFPS());
            DrawText(fps ,0 ,0 ,5, DARKGREEN);
        EndDrawing();
    }

    UnloadRenderTexture(renderTarget);
    CloseWindow();
    return 0;
}
CollisionResult CheckCameraCollision(Camera camera, Vector3 velocity) {
    BoundingBox camBox = {
        {camera.position.x - 0.7f, camera.position.y - 2.5f, camera.position.z - 0.7f},
        {camera.position.x + 0.7f, camera.position.y + 2.5f, camera.position.z + 0.7f},
    };
    CollisionResult result = {false};
    for(int i = 0; i < NUM_BOXES; i++) {
        if(CheckCollisionBoxes(camBox, wallBoxes[i])) {
            result.collided = true;
            

            // Calculate penetration depth on each axis
            float xPenetration = fminf(
                wallBoxes[i].max.x - camBox.min.x,
                camBox.max.x - wallBoxes[i].min.x
            );
            
            float yPenetration = fminf(
                wallBoxes[i].max.y - camBox.min.y,
                camBox.max.y - wallBoxes[i].min.y
            );
            
            float zPenetration = fminf(
                wallBoxes[i].max.z - camBox.min.z,
                camBox.max.z - wallBoxes[i].min.z
            );
            
            // Find the axis with least penetration (this is the collision normal)
            if(xPenetration < yPenetration && xPenetration < zPenetration) {
                result.normal = (Vector3){ (velocity.x > 0) ? -1.0f : 1.0f, 0.0f, 0.0f };
            } 
            else if(yPenetration < xPenetration && yPenetration < zPenetration) {
                result.normal = (Vector3){ 0.0f, (velocity.y > 0) ? -1.0f : 1.0f, 0.0f };
            } 
            else {
                result.normal = (Vector3){ 0.0f, 0.0f, (velocity.z > 0) ? -1.0f : 1.0f };
            }
            return result;
        }
    }
    return result;
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

    player.moveSpeed = 7.0f;
    player.sprintSpeed = 10.0f;
    player.crouchSpeed = 5.0f;
    player.acc = 40.0f;

    player.g = 20.0f;
    player.jumpStrength = 6.0f;

    return player;
}
void UpdatePlayer(Player *player, int cameraMode) {
    Vector3 oldPos = player->camera.position;
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
        if(player->isMoving)
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
    Vector3 newPosition = player->camera.position;
    newPosition.x += player->velocity.x * dTime;
    newPosition.y += player->velocity.y * dTime;
    newPosition.z += player->velocity.z * dTime;
    
    Camera tempCam = player->camera;
    tempCam.position = newPosition;

    CollisionResult collision = CheckCameraCollision(tempCam, player->velocity);
    if (collision.collided) {
        // Remove velocity in the direction of the collision normal
        float dot = Vector3DotProduct(player->velocity, collision.normal);
        player->velocity = Vector3Subtract(player->velocity, Vector3Scale(collision.normal, dot));

        float pushOut = 0.01f;
        //player->camera.position = Vector3Add(player->camera.position, Vector3Scale(collision.normal, pushOut));

        // Recalculate position based on adjusted velocity
        newPosition = player->camera.position;
        newPosition.x += player->velocity.x * dTime;
        newPosition.y += player->velocity.y * dTime;
        newPosition.z += player->velocity.z * dTime;

        tempCam.position = newPosition;

        // Only apply if no secondary collision
        if (!CheckCameraCollision(tempCam, player->velocity).collided) {
            player->camera.position = newPosition;
        } else {
            // Stop all movement if still colliding
            player->velocity = (Vector3){0};
        }
    } else {
        player->camera.position = newPosition;
    }


    // apply crouch change to camera y position height
    float crouchY = player->isCrouching ? player->crouchHeight : player->baseHeight;
    if(!(player->isJumping)) {
        player->camera.position.y += (crouchY - player->camera.position.y) * 10.0f * dTime;
    }       
    
    // detect movement
    player->isMoving = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) ||
                        IsKeyDown(KEY_S) || IsKeyDown(KEY_D);
    //UpdateCamera(&player->camera, cameraMode);

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
} // 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