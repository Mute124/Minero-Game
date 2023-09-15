/*
    Bugs:
        Shaders are not fully working. Needs to be fixed

        Start button doesnt show, Main menu temporarily skips until fixed.

    Todo :

      Implement a system so the objects can have multiple shaders

      Add a day night cycle, this can be done using the Ambient color, and using a billboard for the sun


    Documentation :

        GamePlay :

            Movement keybinds :
                Movement : "WASD"

                To look around : Use either arrow keys or your mouse.

                Fly upwards : Space

                Fly Downwards : C or Ctl

            Blocks :

                Break Block : Left Click

                Place Block : Right Click




            Debug :
                Force Quit : Backspace

                Take Screenshot : F9

                VBOS View : F8

                Display HeightMap : F7

                Fullscreen : F4

                Use Application buffer : F5

                Change Display Resolution : F3

                Lock to 60FPS : F2

                Speed up time :  LShift

                Enable Cursor : Left Alt

                Camera Keybinds :

                    Free : 1

                    First Person : 2

                    Third Person : 3

                    Orbital : 4

                    Change Camera Type (Ortho <-> Perspective ) : P
*/

// Run Args
bool SkipMainMenu = true;

// Raylib Stuff.

#include "lib/Buttons/src/ButtonR.h" // Needed for buttons.
#include "lib/raylib.h"
#include "lib/raymath.h"
#include "lib/rcamera.h"
#include "lib/rlgl.h"

// Object classes
#include "Classes/Monobehavior.h"
#include "Classes/Block.h"

#include "Classes/GUtils.h"
#include "Classes/Gameobj.h"
#include "Classes/Globals.h"
#include "Classes/MenuCamera.h"
#include "Classes/Player/Player.h"
#include "Classes/TestPlane.h"
#include "Classes/Logman.h"
#include "Classes/Vehicles/Plane.h"
#include "Classes/World/WorldFloor.h"

#include "Classes/HeightMap.h"
#include "Classes/Flashlight.h"
#include "Classes/Gun.h"
#include "Classes/Metal_Block.h"

#include "Classes/Graphics/FBO.h"

// Externs required for functionality
// In the lib file
#include "lib/external/glfw/deps/glad/gl.h"

// default libs
#include <algorithm> // needed for Find()
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector> // needed for game object list
#include <thread>
#include <future>

#include <fstream>
#include <iostream>

#define RLIGHTS_IMPLEMENTATION
#include "lib/rlights.h"

// This translates the current screen
typedef enum
{
  Main = 0,
  PauseMenu,
  Gameplay

} GameScreen;

// This is a struct that is a utility for the terrain generator. Dont touch. It
// is mainly used within the GenX, Y, and Z funcs

// Get pixel data from image as a Color struct array // Generate GPU mipmaps for
// a texture Generate cubemap (6 faces) from equirectangular (panorama) texture

// gets the distance from the camera to well, the world

// Audio Functions
void PlayFootstepSound(void *data); // Plays a sound for footsteps

// Physics functions
bool CheckCollisionRayBox(Ray ray, BoundingBox box, float *outDistance);

// Custom Functions

// random float between two values

Vector3 positions[WORLD_SIZE] = {0}; // not used right now


Ray ray; // Player View Ray. used for block breaking

// Custom timing variables

float position = 0.0f; // Circle position
bool pause = false;    // Pause control flag

int targetFPS = 60; // This is what is used to calculate DeltaTime

int main(void)
{

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  Logman::CustomLog(LOG_INFO, "Log man Created. ", NULL);
  GameScreen currentScreen = Main;

  // These store the center X and Y. Vector2 didnt work for some reason, idk
  const int middlex = (screenWidth / 2);
  const int middley = (screenHeight / 2);

  SetConfigFlags(FLAG_MSAA_4X_HINT ); // states the window can be resized and
                                                             // allows anti-aliasing if available
  InitWindow(GetScreenWidth(), GetScreenHeight(), "Minero");

  ButtonR *mmen_start =
      new ButtonR("start", middlex, middley); // Main start button

  MenuCamera *menucamera = new MenuCamera(); // camera for the main menu

  InitAudioDevice(); // starts the audio driver(s).

  DisableEventWaiting();     // Enables the ability for the game to run even if a
                             // thing is lagging. Like for example if terrain is
                             // still generating, it wont pause the game.
  SetExitKey(KEY_BACKSPACE); // In event of your fuck up press this.
  // Load skybox model
  bool HasDone;

  if (SkipMainMenu != true)
  {
    Logman::CustomLog(LOG_INFO, "Main loading done, Main menu drawing", NULL);

    // Main menu Loop
    while (currentScreen == Main && !WindowShouldClose())
    {

      BeginDrawing();

      ClearBackground(WHITE);

      mmen_start->draw(); // draws the start button
      EndDrawing();

      // key to go to the game incase button kaput
      if (IsKeyPressed(KEY_DELETE))
      {
        currentScreen = Gameplay;
      }

      // Main Menu Start Button
      if (mmen_start->IsClicked())
      {
        currentScreen = Gameplay;
      }
    }
  }
  delete mmen_start;
  // Define transforms to be uploaded to GPU for instances

  // Load postprocessing shader
  // NOTE: Defining 0 (NULL) for vertex shader forces usage of internal default
  // vertex shader

  //  std::vector<GameObject *>
  //      objects; // Declares the main list for all game objects. Do not touch or
  // bad things will happen. Like memory leaks :)

  Model DefaultBlockModel = LoadModelFromMesh(
      GenMeshCube(BLOCK_SIZE, BLOCK_SIZE,
                  BLOCK_SIZE)); // this is the default model used in blocks.

  Shader postProcessShader = LoadShader(
      TextFormat("resources/shaders/glsl330/lighting.vs", GLSL_VERSION),
      TextFormat("resources/shaders/glsl330/lighting.fs", GLSL_VERSION));

  postProcessShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(postProcessShader, "viewPos");

  postProcessShader.locs[SHADER_LOC_COLOR_AMBIENT] = GetShaderLocation(postProcessShader, "ambient");

  // Set shader value: ambient light level
  int ambientLoc = GetShaderLocation(postProcessShader, "ambient");
  static const float ambientColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  SetShaderValue(postProcessShader, ambientLoc, ambientColor, SHADER_UNIFORM_VEC4);

  SetShaderValue(postProcessShader, postProcessShader.locs[SHADER_LOC_COLOR_AMBIENT], ambientColor, SHADER_UNIFORM_VEC4);
  // Player Object Creation
  const int Playersize = 3.f;
  Model PlayerModel = LoadModelFromMesh(GenMeshCube(
      3.0f, 3.0f,
      3.0f)); // Generates the playermodel. For right now it is a cube.
  Player *player = new Player(
      Vector3{0.0f, 2.0f, 4.0f}, 100, PlayerModel,
      CAMERA_FIRST_PERSON); // Player object creation, Read the Player()
                            // function for other details. It is in Player.h
  // Adds the player obj to the list of Game objects so it gets freed
  // automatically. Neat aint it :)
  GameObject::PushObject(player);
  
  // Block Initialization

  Block *block = new Block(BlockDirt, Vector3Zero(), WHITE,
                           postProcessShader, LoadModel("resources/models/Block.obj"));
  GameObject::PushObject(block);

  int PostViewLoc = GetShaderLocation(postProcessShader, "viewPos");

  // HeightMap *heightmap = new HeightMap(800, 600, 0, 0, 1.0f);

  // Mesh hmap = GenMeshHeightmap(heightmap->GetHeightMap(), Vector3{5.0f, 5.0f, 5.0f});

  /*
                                                  Lights
  _________________________________________________________________________________________________________


      Lights show up on the floor but the sky is just a disgusting piss yellow.
  There should be the skybox.

      Ocean for some fucking reason gets smaller when you get further away from
  land. why? because it is special

  */

  // Create one light

  // Texture2D texture = LoadTextureFromImage(heightmap->GetHeightMap());

  // Texture2D Base = LoadTextureFromImage(heightmap->GetHeightMap());

  // UnloadTexture(Base);

  // Load skybox model
  // skybox creation.
  Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
  Model skybox = LoadModelFromMesh(cube);

  bool useHDR = true;

  // Load skybox shader and set required locations
  // NOTE: Some locations are automatically set at shader loading
  skybox.materials[0].shader =
      LoadShader("resources/skybox.vs", "resources/skybox.fs");

  // since the compiler complains about the references of such, these three vars
  // are for the skybox shaders. They will be deleted after.
  static int a = 7;
  static int b = {useHDR ? 1 : 0};
  static int c = 0;

  SetShaderValue(
      skybox.materials[0].shader,
      GetShaderLocation(skybox.materials[0].shader, "environmentMap"), &a,
      SHADER_UNIFORM_INT);
  SetShaderValue(skybox.materials[0].shader,
                 GetShaderLocation(skybox.materials[0].shader, "doGamma"), &b,
                 SHADER_UNIFORM_INT);
  SetShaderValue(skybox.materials[0].shader,
                 GetShaderLocation(skybox.materials[0].shader, "vflipped"), &b,
                 SHADER_UNIFORM_INT);

  // Load cubemap shader and setup required shader locations
  Shader shdrCubemap =
      LoadShader("resources/cubemap.vs", "resources/cubemap.fs");

  SetShaderValue(shdrCubemap,
                 GetShaderLocation(shdrCubemap, "equirectangularMap"), &c,
                 SHADER_UNIFORM_INT);

  char skyboxFileName[256] = {0};

  Texture2D panorama;


  if (useHDR)
  {
    TextCopy(skyboxFileName, "resources/daytime.hdr");

    // Load HDR panorama (sphere) texture
    panorama = LoadTexture(skyboxFileName);

    // Generate cubemap (texture with 6 quads-cube-mapping) from panorama HDR
    // texture NOTE 1: New texture is generated rendering to texture, shader
    // calculates the sphere->cube coordinates mapping NOTE 2: It seems on some
    // Android devices WebGL, fbo does not properly support a FLOAT-based
    // attachment, despite texture can be successfully created.. so using
    // PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 instead of
    // PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
    skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = Global::MeshTools::GenTextureCubemap(
        shdrCubemap, panorama, 1024, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    UnloadTexture(panorama); // Texture not required anymore, cubemap
    // already generated
  }
  else
  {
    Image img = LoadImage("resources/skybox.png");
    skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(
        img, CUBEMAP_LAYOUT_AUTO_DETECT); // CUBEMAP_LAYOUT_PANORAMA
    UnloadImage(img);
  }

  player->model.materials[0].shader = postProcessShader;
  postProcessShader.locs[SHADER_LOC_MATRIX_MODEL] =
      GetShaderLocation(postProcessShader, "matModel");
  float angle = 6.282f;
  float radius = 100.0f;
  Light Sun[2] = {0};

  // Sun creation
  Sun[0] = CreateLight(LIGHT_DIRECTIONAL, Vector3{50.0f, 50.0f, 50.0f},
                       Vector3Zero(), WHITE, postProcessShader);
  Sun[1] = CreateLight(LIGHT_POINT, Vector3{50.0f, 50.0f, 50.0f}, Vector3Zero(),
                       WHITE, postProcessShader);

  DisableCursor(); // Limit cursor to relative movement inside the window

  // Assignment of shaders

  //--------------------------------------------------------------------------------------
  Image PerlinTest = GenImagePerlinNoise(1000, 1000, 0, 0, 4.0f);

  ExportImage(PerlinTest, "PerlinTest.png");

  Image image = LoadImage("PerlinTest.png");
  Texture2D texture2 =
      LoadTextureFromImage(image); // Convert image to texture (VRAM)

  Mesh mesh = GenMeshHeightmap(
      image, Vector3{16, 8, 16});        // Generate heightmap mesh (RAM and VRAM)
  Model model = LoadModelFromMesh(mesh); // Load model from generated mesh

  UnloadImage(
      image); // Unload heightmap image from RAM, already uploaded to VRAM

  Color skyColor = SKYBLUE;
  Color groundColor = GRAY;
  // ambient light level
  int amb = GetShaderLocation(postProcessShader, "ambient");
  static float shaderamb[4] = {5.0f, 5.0f, 5.0f, 5.0f};
  SetShaderValue(postProcessShader, amb, shaderamb, UNIFORM_VEC4);

  Flashlight *flashlight = new Flashlight(player->getSelfCamera(), postProcessShader);

  Vector3 lightDirection[3] = {
      Sun[0].target, Sun[1].target, flashlight->GetLight().target}; // Example light direction pointing upwards
  SetShaderValue(postProcessShader, Sun[0].targetLoc, &lightDirection[0],
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(postProcessShader, Sun[1].targetLoc, &lightDirection[1],
                 SHADER_UNIFORM_VEC3);
  SetShaderValue(postProcessShader, flashlight->GetLight().targetLoc, &lightDirection[1],
                 SHADER_UNIFORM_VEC3);

  SetShaderValue(postProcessShader,
                 GetShaderLocation(postProcessShader, "skyColor"), &skyColor,
                 SHADER_UNIFORM_VEC4);
  SetShaderValue(postProcessShader,
                 GetShaderLocation(postProcessShader, "groundColor"),
                 &groundColor, SHADER_UNIFORM_VEC4);

  SetShaderValue(postProcessShader,
                 GetShaderLocation(postProcessShader, "ambientColor"), &amb,
                 SHADER_UNIFORM_VEC4);

  // Main game loop
  HasDone = false;
  bool CreateSunRay = true;

  Vector3 Orgin =
      Vector3{player->getPosition().x, player->getPosition().y - 10.0f,
              player->getPosition().z};
  Logman::CustomLog(LOG_DEBUG, "Trace ", NULL);
  Vector3 testblock = Vector3One();

  Logman::CustomLog(LOG_DEBUG, "Generating World...", NULL);

  double Starttime = GetTime();

  // terrain generation should go here

  double current = GetTime();

  Logman::CustomLog(LOG_DEBUG, TextFormat("World generated in %d", Starttime - current),
                    NULL);

  Image Perlin = GenImagePerlinNoise(1000, 1000, 0, 0, 10.0f);
  ExportImage(Perlin, "perlin.png");

  Model terrain = LoadModelFromMesh(GenMeshHeightmap(Perlin, (Vector3){10000.0f, 50.0f, 10000.0f}));

  terrain.materials[0].shader = postProcessShader;

  UnloadImage(Perlin);
  UnloadImage(PerlinTest);
  if (!DirectoryExists("temp"))
  {
    system("mkdir temp");
  }
  Texture tex;

  string Command;

  // GameObject::PushObject(new Plane(postProcessShader, Vector3One(), player));

  //GameObject::PushObject(new WorldFloor(postProcessShader, (Vector3){0.0f, -4.0f, 0.0f}));

  // RenderTexture2D fbo = FBO::LoadRenderTextureDepthTex(screenWidth, screenHeight);

  
  // All setup goes above!

  SetTargetFPS(60);
  while (!WindowShouldClose())
  {

    // Pause Menu
    if (IsKeyPressed(KEY_ESCAPE))
    {
      // Todo, move the menuCamera to be created on game startup and then hidden. it gets shown on if statement validation
      bool exit = false;
      bool manualExit = false;

      MenuCamera *men_pause = new MenuCamera();
      ButtonR *save = new ButtonR("Save", 100, 100);
      ButtonR *Options = new ButtonR("Options", 100, 200);
      ButtonR *exitButton = new ButtonR("Exit to Windows", 100, 300);

      int cycle; // not used rn
      EnableCursor();
      while (!exit)
      {
        BeginDrawing();
        DrawText("PAUSE", 100, 100, 20, RED);
        save->draw();
        save->update();

        Options->draw();
        Options->update();

        exitButton->draw();
        exitButton->update();
        if (save->IsClicked())
        {

          // todo : implement Saving game data, probably use the Objects vector.
        }

        if (exitButton->IsClicked())
        {
          manualExit = true;
        }

        if (Options->IsClicked())
        {
          bool closeOptions = false;
          while (!closeOptions)
          {
            // TODO : Add options

            if (IsKeyDown(KEY_ESCAPE))
            {
              closeOptions = true;
            }
          }
        }

        EndDrawing();
        if (IsKeyPressed(KEY_ESCAPE) && exit == false)
        {
          exit = true;
        }
        else if (IsKeyPressed(KEY_ESCAPE) && exit == true)
        {
          break;
        }

        if (manualExit)
        {
          exit = true;
        }
      }
      // cleanup
      UnloadTexture(tex);
      delete men_pause;
      delete save;
      delete exitButton;
      if (manualExit)
      {

        Logman::CustomLog(LOG_INFO, "Exiting Game", NULL);
        break;
      }
    }

    // update ray
    ray.position = player->getPosition();
    ray.direction = player->getTarget();

    // Show Debug shit
    if (IsKeyDown(KEY_END))
    {
      BeginDrawing();
      BeginMode3D(player->getSelfCamera());
      // Sun Rays
      DrawLine3D(Sun[0].position, player->getPosition(), RED);
      EndMode3D();
      EndDrawing();
    }

    // to do :
    // Update variables that are declared in the settings.ini file

    UpdateCamera(
        player->getSelfCameraPointer(),
        player
            ->cameraMode); // Updates the camera with the current mode and data.

    // Keybinds yah

    // Check for left click

    if (IsKeyPressed(KEY_F))
    {
      flashlight->Switch();
    }

    // If statement for block placing.
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
      Vector3 placepos;
      placepos.x = round(player->getTarget().x);
      placepos.y = round(player->getTarget().y);
      placepos.z = round(player->getTarget().z);

      GameObject::PushObject(new Block(BlockStone, placepos, WHITE, postProcessShader, DefaultBlockModel));
    }

    if (!IsKeyDown(KEY_LEFT_ALT))
    {
      if (!IsCursorHidden())
      {
        DisableCursor();
      }
      UpdateCamera(player->getSelfCameraPointer(), player->cameraMode); // Update camera
    }
    else
    {
      EnableCursor();
    }

    if (IsKeyPressed(KEY_F9))
    {
      // take a screenshot
      for (int i = 0; i < INT_MAX; i++)
      {
        const char *fileName = TextFormat("screen%i.png", i);
        // if there isnt a duplicate file, it then will create a screenshot.
        if (FileExists(fileName) == 0)
        {
          TakeScreenshot(fileName);
          break;
        }
      }
    }

    if (IsKeyDown(KEY_SPACE)) {
      Logman::CustomLog(LOG_TRACE, "What...", 0);
      //player->SetPosition((Vector3){player->getPosition().x, player->getPosition().y + 1.0f, player->getPosition().z});
      UpdateCameraPro(player->getSelfCameraPointer(), Vector3One(), Vector3Zero(), 1.0f);
    }

    if (IsKeyDown(KEY_C)) {
      player->SetPosition((Vector3){player->getPosition().x, player->getPosition().y - 0.1f, player->getPosition().z});
    }
    
    //----------------------------------------------------------------------------------

    PollInputEvents(); // helps for some reason?

    // Update the light shader with the camera view position
    // Update the shader with the camera view vector (points towards { 0.0f,
    // 0.0f, 0.0f })

    float cameraPos[3] = {player->getPosition().x,
                          player->getPosition().y,
                          player->getPosition().z};

    Vector3 SunPos = {Sun[0].position.x, Sun[0].position.y, Sun[0].position.z};

    UpdateLightValues(postProcessShader, Sun[0]);
    UpdateLightValues(postProcessShader, Sun[1]);

    flashlight->Update(postProcessShader, player->getPosition(), player->getPosition());

    SetShaderValue(postProcessShader, postProcessShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(player->getSelfCamera());

      rlDisableBackfaceCulling();
      rlDisableDepthMask();
        DrawModel(skybox, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
      rlEnableBackfaceCulling();
      rlEnableDepthMask();

    DrawModel(terrain, Vector3Zero(), 1.0f, GREEN); // not needed atm, if it isnt for a while, it will get deleted
    //  We are inside the cube, we need to disable backface culling!

    BeginShaderMode(postProcessShader);
    GameObject::Render();

    // Draw spheres to show where the lights are
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
      if (Sun[i].enabled)
        DrawSphereEx(Sun[i].position, 0.2f, 8, 8, Sun[i].color);
      else
        DrawSphereWires(Sun[i].position, 0.2f, 8, 8,
                        ColorAlpha(Sun[i].color, 0.3f));
    }



    // Get the mouse ray based on the current camera position and mouse position
    Vector2 mouse_pos = GetMousePosition();
    ray = GetMouseRay(mouse_pos, player->getSelfCamera());

    

    EndMode3D();
    EndShaderMode();
    // Draw info boxes
    DrawRectangle(5, 5, 330, 100, Fade(SKYBLUE, 0.5f));
    DrawRectangleLines(5, 5, 330, 100, BLUE);

    DrawText(TextFormat("FPS : %i", GetFPS()), 15, 110, 10, GREEN);
    DrawText(TextFormat("Time :  %i", GetTime()), 15, 140, 10, GREEN);

    DrawRectangle(600, 5, 195, 100, Fade(SKYBLUE, 0.5f));
    DrawRectangleLines(600, 5, 195, 100, BLUE);

    DrawText("Camera status:", 610, 15, 10, BLACK);
    DrawText(TextFormat(
                 "- Mode: %s",
                 (player->cameraMode == CAMERA_FREE)           ? "FREE"
                 : (player->cameraMode == CAMERA_FIRST_PERSON) ? "FIRST_PERSON"
                 : (player->cameraMode == CAMERA_THIRD_PERSON) ? "THIRD_PERSON"
                 : (player->cameraMode == CAMERA_ORBITAL)      ? "ORBITAL"
                                                               : "CUSTOM"),
             610, 30, 10, BLACK);
    DrawText(TextFormat("- Projection: %s",
                        (player->getProjection() == CAMERA_PERSPECTIVE)
                            ? "PERSPECTIVE"
                        : (player->getProjection() == CAMERA_ORTHOGRAPHIC)
                            ? "ORTHOGRAPHIC"
                            : "CUSTOM"),
             610, 45, 10, BLACK);
    DrawText(TextFormat("- Position: (%06.3f, %06.3f, %06.3f)",
                        player->getPosition().x,
                        player->getPosition().y,
                        player->getPosition().z),
             610, 60, 10, BLACK);
    DrawText(TextFormat("- Target: (%06.3f, %06.3f, %06.3f)",
                        player->getPosition().x, player->getPosition().y,
                        player->getPosition().z),
             610, 75, 10, BLACK);
    DrawText(TextFormat("- Up: (%06.3f, %06.3f, %06.3f)", player->getup().x,
                        player->getup().y, player->getup().z),
             610, 90, 10, BLACK);

    //(fbo, (Rectangle){0, 0, screenWidth, -screenHeight}, (Vector2){middlex, middley}, WHITE);
    EndDrawing();

    // check if we are using hdr

    /*

    */
  }

ExitGame:
  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  // UnloadRenderTexture(fbo);
  CloseAudioDevice();

  //--------------------------------------------------------------------------------------

  // Unload Models/Textures
  // UnloadTexture(texture); // Unload texture
  UnloadModel(model); // Unload model
  UnloadModel(DefaultBlockModel);

  UnloadShader(postProcessShader);

  UnloadImage(PerlinTest);
  UnloadImage(image);
  using namespace std;
  {
    remove("PerlinTest.png");
    remove("HMap.png");
  }

  delete player;
  delete mmen_start; // Deletes the main menu
  delete menucamera;
  delete block;
  GameObject::FlushBuffer();
  // delete heightmap;

  delete flashlight;

  return 0;
}

/*
                                            Code Graveyard
_________________________________________________________________________________________________________________________

Wow such empty





*/