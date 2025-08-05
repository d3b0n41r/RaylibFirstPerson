#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "collisions.h"

#define GLSL_VERSION 330

#define LORENDER_WIDTH 640
#define LORENDER_HEIGHT 480
#define HIRENDER_WIDTH 1920
#define HIRENDER_HEIGHT 1080

typedef enum {
    IDLE,
    WALKING,
    RUNNING,
    CROUCHING,
    JUMPING,
    FALLING
} MovementState;

typedef struct Player {
    MovementState movementState;

    Camera3D camera;

    Model model;

    Vector3 velocity; 
    bool isMoving;
    bool isSprinting;
    bool isCrouching;
    bool isJumping;

    float baseFOV;
    float sprintFOV;
    float crouchFOV;
    float fovSpeed;// fov transition smoothing

    float targetCrouchOffset;
    float currentCrouchOffset;

    float moveSpeed;
    float sprintSpeed;
    float crouchSpeed;
    float acc; // acceleratio for smoothing between states
    float g; // gravity
    float jumpStrength;

    float walkBobAmount;
    float sprintBobAmount;
    float crouchBobAmount;

    Collider collider;

    float bobbingTime;
    float bobbingSpeed;
    float bobbingAmount;
    float swayAmount; // makes bobbing a bit more smooth
} Player;


Player InitPlayer(Model characterModel, Model collisionModel);
void UpdatePlayer(Player *player, int cameraMode, const int NumColliders, Collider colliders[]);

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT); 
    // initialise window
    InitWindow(1920, 1080, "slashcast");
    SetTargetFPS(60);
    SetExitKey(-1); // disables closure if ESC pressed
    SetWindowState(FLAG_FULLSCREEN_MODE);


    // initialise base model
    Model character = LoadModel("../assets/models/MaleBase.glb");
    Model characterCollision = LoadModel("../assets/models/MaleBaseCollision.glb");
    // Texture2D texture = LoadTexture("assets/character_texture.png");
    // character.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;

    // initialise a new player
    Player player = InitPlayer(character, characterCollision);
    int cameraMode = CAMERA_FIRST_PERSON;
    DisableCursor();

    // shaders
    Shader posterization = LoadShader(0, TextFormat("../assets/shaders/posterization.fs", GLSL_VERSION));
    Shader shader = LoadShader(TextFormat("../assets/shaders/skybox.vs", GLSL_VERSION),
                                            TextFormat("../assets/shaders/skybox.fs", GLSL_VERSION));

    // test objects
    Model Cylinder = LoadModel("../assets/models/cylinder.obj");
    Model Floor = LoadModel("../assets/models/cube.obj");
    Model Ramp = LoadModel("../assets/models/Ramp.obj");
    Model Sphere = LoadModel("../assets/models/sphere.obj");
    //Cylinder material setup
	//Cylinder.materials[0].shader = shader;
	Cylinder.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
    //Floor material setup
    //Floor.materials[0].shader = shader;
    Floor.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GRAY;
    //Ramp material setup
    //Ramp.materials[0].shader = shader;
    Ramp.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
    //Sphere material setup
    //Sphere.materials[0].shader = shader;
    Sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

    // skybox stuff
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Model skybox = LoadModelFromMesh(cube);
    skybox.materials[0].shader = shader;
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "environmentMap"), (int[1]){ MATERIAL_MAP_CUBEMAP }, SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "doGamma"), (int[1]) { 0 }, SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "vflipped"), (int[1]){ 0 }, SHADER_UNIFORM_INT);
    Shader shdrCubemap = LoadShader(TextFormat("../assets/shaders/cubemap.vs", GLSL_VERSION),
                                    TextFormat("../assets/shaders/cubemap.fs", GLSL_VERSION));
    SetShaderValue(shdrCubemap, GetShaderLocation(shdrCubemap, "equirectangularMap"), (int[1]){ 0 }, SHADER_UNIFORM_INT);
    Image img = LoadImage("../assets/cubemaps/Cubemap_Sky_23-512x512.png");
    skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);    // CUBEMAP_LAYOUT_PANORAMA
    UnloadImage(img);

    // initialise binds
    FILE *file = fopen("../config/keybinds.ini", "r");
    /*fscanf(file, "moveForward=%d\n", &keybinds.moveForward);
    repeat for other keys
    fclose(file);*/
    
    // collision
    
    const int NumColliders = 4;
    Collider colliders[NumColliders]; // collider array
    Vector3 positions[NumColliders]; // positions of colliders array
    // set positions of the colliders
    positions[0] = (Vector3){-2, 1.0f, -2};
	positions[1] = (Vector3){2, 1, 2};
	positions[2] = (Vector3){0, 2, 0};
	positions[3] = (Vector3){2, 2, -1};
    //Setup all the collider for the meshes
	SetupColliderMesh(&colliders[0], Ramp.meshes[0]);
	SetupColliderMesh(&colliders[1], Cylinder.meshes[0]);
	SetupColliderMesh(&colliders[2], Floor.meshes[0]);
	SetupColliderMesh(&colliders[3], Sphere.meshes[0]);
    //Update the colliders
    for(int i = 0; i < NumColliders;i++)UpdateCollider(positions[i],&colliders[i]);

    RenderTexture2D renderTarget = LoadRenderTexture(LORENDER_WIDTH, LORENDER_HEIGHT);
    // main game loop
    while (!WindowShouldClose()) {
        UpdatePlayer(&player, cameraMode, NumColliders, colliders);
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

                // just some test models
                DrawModel(Floor,(Vector3){0,0,0},1,WHITE);
                DrawModel(Cylinder,(Vector3){2,0,2},1,WHITE);
                DrawModel(Ramp,(Vector3){-2,0,-2},1,WHITE);
                DrawModel(Sphere,(Vector3){2,0,-1},1,WHITE);

                // draw player
                Vector3 bodyPos = player.camera.position;
                bodyPos.y -= 2.0f; //body pos at eye level instead of starting from feet
                //get camera forward vector
                Vector3 forwardCam = Vector3Normalize((Vector3){
                    player.camera.target.x - player.camera.position.x,
                    0,
                    player.camera.target.z - player.camera.position.z
                });
                bodyPos = Vector3Add(bodyPos, Vector3Scale(forwardCam, -0.1)); //-0.1f puts camera on eyes instead of camera inside of head
                float camYaw = atan2f(forwardCam.x, forwardCam.z) * RAD2DEG;
                DrawModelEx(player.model, bodyPos, (Vector3){ 0.0f, 5.0f, 0.0f}, camYaw, (Vector3){ 1.0f, 1.0f, 1.0f }, BLACK);
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
        EndDrawing();
    }

    UnloadRenderTexture(renderTarget);
    //Clear up all the data
    //Shaders
    UnloadShader(shader);
    //Models
    UnloadModel(Floor);
    UnloadModel(Cylinder);
    UnloadModel(Ramp);
    UnloadModel(Sphere);
    //Free the colliders
    for(int i = 0; i < NumColliders;i++)UnloadCollider(&colliders[i]);
    CloseWindow();
    return 0;
}
Player InitPlayer(Model characterModel, Model collisionModel) {
    Player player = { 0 }; // create new player

    // initialise camera on player eyelevel
    player.camera.position = (Vector3){ 0.0f, 6.0f, 0.0f }; // camera position
    player.camera.target = (Vector3){ 0.0f, 2.0f, 0.0f }; // camerea looking at point
    player.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f }; // up vector (rotation towards target)
    player.camera.fovy = 100.0f;
    player.camera.projection = CAMERA_PERSPECTIVE; // camera projection type

    player.baseFOV = 100.0f;
    player.sprintFOV = 110.0f;
    player.crouchFOV = 90.0f;
    player.fovSpeed = 5.0f; // fov transition smoothing

    // bobbing when walking
    player.bobbingTime = 0.0f;
    player.bobbingSpeed = 10.0f;
    player.swayAmount = 0.01; // change this for swaying amount customisation

    player.targetCrouchOffset = 0.0f;
    player.currentCrouchOffset = 0.0f;

    player.walkBobAmount = 0.03f;
    player.sprintBobAmount = 0.06;
    player.crouchBobAmount = 0.00;

    player.moveSpeed = 5.0f;
    player.sprintSpeed = 7.0f;
    player.crouchSpeed = 4.0f;
    player.acc = 40.0f;

    player.model = characterModel;
    Collider playerCollider;
    SetupColliderMesh(&playerCollider, collisionModel.meshes[0]);
    player.collider = playerCollider;

    player.g = 20.0f;
    player.jumpStrength = 7.0f;

    return player;
}
//TODO - when holding two or more keys while spam jumping theres a delay between jumps
// 
// slide down ramp when crouch
//   
void UpdatePlayer(Player *player, int cameraMode, const int NumColliders, Collider colliders[]) {
    Vector3 oldPos = player->camera.position;
    float dTime = GetFrameTime(); //time in seconds for last frame drawn (delta time)

    // detect movement type (sprinting or crouch walking)
    player->isSprinting = IsKeyDown(KEY_LEFT_SHIFT);
    player->isCrouching = IsKeyDown(KEY_LEFT_CONTROL) && !player->isJumping;

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
    player->bobbingAmount = player->walkBobAmount;
    float speed = player->moveSpeed;
    float FOV = player->baseFOV;
    if(player->isSprinting) {
        speed = player->sprintSpeed;
        if(player->isMoving) {
            FOV = player->sprintFOV;
            player->bobbingAmount = player->sprintBobAmount;
        }
    }
    if(player->isCrouching) {
        speed = player->crouchSpeed;
        FOV = player->crouchFOV;
        player->bobbingAmount = player->crouchBobAmount;
    }

    // get forward vector, use horizontal components for world vector so speed doesnt change with camera rotation and yaw
    Vector3 forward = Vector3Normalize(Vector3Subtract(player->camera.target, player->camera.position));
    Vector3 horizontalForward = Vector3Normalize((Vector3){
        forward.x,
        0.0f,
        forward.z
    });
    // get correct horizontal value
    Vector3 horizontalRight = Vector3Normalize(Vector3CrossProduct(horizontalForward, player->camera.up));
    // calculate world direction using only horizontal components
    Vector3 worldDirection = {
        (horizontalRight.x * dir.x + horizontalForward.x * dir.z),
        0.0f,
        (horizontalRight.z * dir.x + horizontalForward.z * dir.z)
    };
    Vector3 targetVelocity = Vector3Scale(worldDirection, speed);

    // make acceleration look nicer
    player->velocity.x += (targetVelocity.x - player->velocity.x) * player->acc * dTime;
    player->velocity.z += (targetVelocity.z - player->velocity.z) * player->acc * dTime;

    // add gravity to movement
    player->velocity.y -= player->g * dTime;


    // smooth fov changing when changing movement speeds
    player->camera.fovy += (FOV - player->camera.fovy) * player->fovSpeed * dTime;

    // apply movement to camera based on collision
    UpdateCollider(player->camera.position, &player->collider);
    for(int i = 0; i < NumColliders; i++) {
        Vector3 collisionNormal = {0};
        if(CheckCollision(player->collider, colliders[i], &collisionNormal)) {
            player->camera.position = Vector3Add(player->camera.position, collisionNormal);
            UpdateCollider(player->camera.position, &player->collider);
            if(collisionNormal.y > 0.0f) { //0.0f is 90 degree slope (wall) 
                player->velocity.y = 0;
                player->isJumping = false;
            } else{
                if(!player->isJumping) { //for some reason when spawned the collisionNormal.y is 0, this deals with that
                    player->velocity.y = 0;
                }
            }
        
        }
    }

        // jumping
    if(IsKeyPressed(KEY_SPACE) && !(player->isJumping) && !(player->isCrouching)) {
        player->velocity.y = player->jumpStrength;
        player->isJumping = true;
    }

    player->camera.position.x += player->velocity.x * dTime;
    player->camera.position.y += player->velocity.y * dTime;
    player->camera.position.z += player->velocity.z * dTime;

    // apply crouch change to camera y position height
    player->targetCrouchOffset = player->isCrouching ? -0.5f : 0.0f;
    player->currentCrouchOffset += (player->targetCrouchOffset - player->currentCrouchOffset) * 10.0f * dTime;
    player->camera.position.y += player->currentCrouchOffset;

    // jumping
    if(IsKeyPressed(KEY_SPACE) && !(player->isJumping) && !(player->isCrouching)) {
        player->velocity.y = player->jumpStrength;
        player->isJumping = true;
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
}