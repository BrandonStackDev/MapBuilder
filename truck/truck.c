#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
//sudo apt install libusb-1.0-0-dev
#include <libusb-1.0/libusb.h>

#define TARGET_VID 0x054C  // Sony
#define TARGET_PID 0x0CE6  // DualSense Wireless Controller
#define PACKET_SIZE 64

#define MAX_TURN_ANGLE 0.25f //radians

typedef struct {
    int lx;
    int ly;
    int rx;
    int ry;
    float normLX;
    float normLY;
    float normRX;
    float normRY;
    uint8_t buttons1;
    uint8_t buttons2;
    uint8_t buttons3;
    int dpad;
    int btnSquare;
    int btnCross;
    int btnCircle;
    int btnTriangle;
    int l2;
    int r2;
} ControllerData;


void TakeScreenshotWithTimestamp(void) {
    // Get timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[128];
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.png", t);

    // Save the screenshot
    TakeScreenshot(filename);
    TraceLog(LOG_INFO, "Saved screenshot: %s", filename);
}

Vector3 RotateY(Vector3 v, float angle) {
    float cs = cosf(angle);
    float sn = sinf(angle);
    return (Vector3){
        v.x * cs - v.z * sn,
        v.y,
        v.x * sn + v.z * cs
    };
}

void print_bytes(unsigned char *data, int len) {
    printf("HEX: ");
    for (int i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("\nBIN: ");
    for (int i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            printf("%d", (data[i] >> b) & 1);
        }
        printf(" ");
    }
    printf("\nASCII: ");
    for (int i = 0; i < len; i++) printf("%c", data[i] >= 32 && data[i] < 127 ? data[i] : '.');
    printf("\n\n");
}

ControllerData ParseDualSenseInput(uint8_t *report, int length) {
    if (length < 64 || report[0] != 0x01) {
        printf("Invalid report\n");
        //return;
    }

    int lx = report[1];
    int ly = report[2];
    int rx = report[3];
    int ry = report[4];

    float normLX = (lx - 128) / 127.0f;
    float normLY = (ly - 128) / 127.0f;
    float normRX = (rx - 128) / 127.0f;
    float normRY = (ry - 128) / 127.0f;

    printf("Left Stick:  X=%d Y=%d  (%.2f, %.2f)\n", lx, ly, normLX, normLY);
    printf("Right Stick: X=%d Y=%d  (%.2f, %.2f)\n", rx, ry, normRX, normRY);

    uint8_t buttons1 = report[7];
    uint8_t buttons2 = report[8];
    uint8_t buttons3 = report[9];

    int dpad = buttons1 & 0x0F;
    int btnSquare  = (buttons2 & 0x10) > 0;
    int btnCross   = (buttons2 & 0x20) > 0;
    int btnCircle  = (buttons2 & 0x40) > 0;
    int btnTriangle= (buttons2 & 0x80) > 0;

    printf("D-Pad: %d | Square: %d Cross: %d Circle: %d Triangle: %d\n",
           dpad, btnSquare, btnCross, btnCircle, btnTriangle);

    int l2 = report[10];
    int r2 = report[11];
    printf("L2: %d, R2: %d\n", l2, r2);
    return (ControllerData){lx,ly,rx,ry,normLX,normLY,normRX,normRY,buttons1,buttons2,buttons3,dpad,btnSquare,btnCross,btnCircle,btnTriangle,l2,r2};
}

int main(void)
{
    //setup gamepad
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    libusb_device_handle *handle = NULL;
    ssize_t count;
    int r;
    bool contInvertY = true;

    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(r));
        return 1;
    }

    count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        fprintf(stderr, "Error getting device list\n");
        libusb_exit(ctx);
        return 1;
    }

    printf("Found %ld USB devices:\n", count);
    for (ssize_t i = 0; i < count; i++) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor desc;

        libusb_get_device_descriptor(dev, &desc);

        printf("Device %zu: VID: %04X, PID: %04X\n", i, desc.idVendor, desc.idProduct);

        libusb_device_handle *handle;
        if (libusb_open(dev, &handle) == 0) {
            unsigned char buffer[256];

            // Try to read the manufacturer string
            if (desc.iManufacturer) {
                libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, buffer, sizeof(buffer));
                printf("  Manufacturer: %s\n", buffer);
            }
            if (desc.iProduct) {
                libusb_get_string_descriptor_ascii(handle, desc.iProduct, buffer, sizeof(buffer));
                printf("  Product     : %s\n", buffer);
            }
            if (desc.idVendor) {
                libusb_get_string_descriptor_ascii(handle, desc.idVendor, buffer, sizeof(buffer));
                printf("  Vendor id    : %s\n", buffer);
            }
            if (desc.idProduct) {
                libusb_get_string_descriptor_ascii(handle, desc.idProduct, buffer, sizeof(buffer));
                printf("  Product id   : %s\n", buffer);
            }

            libusb_close(handle);
        }
    }

    printf("Scanning for device VID: %04X, PID: %04X...\n", TARGET_VID, TARGET_PID);
    int found = 0;
    libusb_device *gp_dev;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;

        if (libusb_get_device_descriptor(device, &desc) == 0) {
            if (desc.idVendor == TARGET_VID && desc.idProduct == TARGET_PID) {
                printf("Device found! Opening...\n");
                r = libusb_open(device, &handle);
                if (r == 0 && handle != NULL) {
                    printf("Device opened successfully!\n");
                    gp_dev = device;
                    found = 1;
                    break;
                } else {
                    fprintf(stderr, "Failed to open device: %s\n", libusb_error_name(r));
                    break;
                }
            }
        }
    }

    if (!found) {
        fprintf(stderr, "Target device not found (VID: %04X, PID: %04X)\n", TARGET_VID, TARGET_PID);
    }
    else
    {
        //find interface
        struct libusb_config_descriptor *config;
        libusb_get_active_config_descriptor(gp_dev, &config);

        int foundInterface = -1;

        for (int i = 0; i < config->bNumInterfaces; i++) {
            const struct libusb_interface *interface = &config->interface[i];
            for (int j = 0; j < interface->num_altsetting; j++) {
                const struct libusb_interface_descriptor *desc = &interface->altsetting[j];
                if (desc->bInterfaceClass == LIBUSB_CLASS_HID) {
                    foundInterface = desc->bInterfaceNumber;
                    break;
                }
            }
            if (foundInterface != -1) break;
        }

        libusb_free_config_descriptor(config);

        if (foundInterface != -1) {
            printf("Found HID interface: %d\n", foundInterface);
            // Use foundInterface in libusb_kernel_driver_active(), claim, etc.
        } else {
            fprintf(stderr, "No HID interface found.\n");
        }

        // Detach kernel driver if needed
        if (libusb_kernel_driver_active(handle, 3) == 1) {
            printf("Kernel driver active on interface 3, detaching...\n");
            int detachResult = libusb_detach_kernel_driver(handle, 3);
            if (detachResult != 0) {
                fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_error_name(detachResult));
                libusb_close(handle);
                libusb_exit(ctx);
                return 1;
            }
        }


        // Claim HID interface (interface 3)
        int r = libusb_claim_interface(handle, 3);
        if (r < 0) {
            fprintf(stderr, "Failed to claim interface 3: %s\n", libusb_error_name(r));
            libusb_close(handle);
            libusb_exit(ctx);
            return 1;
        }

        // Read from interrupt IN endpoint (0x84)
        unsigned char data[PACKET_SIZE];
        int actual_length = 0;
        r = libusb_interrupt_transfer(handle, 0x84, data, sizeof(data), &actual_length, 1000);
        if (r == 0) {
            printf("Read %d bytes:\n", actual_length);
            for (int i = 0; i < actual_length; i++) {
                printf("%02X ", data[i]);
            }
            printf("\n");
            ParseDualSenseInput(data, PACKET_SIZE);
        } else {
            printf("Read error or timeout: %s\n", libusb_error_name(r));
        }
    }

    int pad_axis = 0;
    bool mouse = false;
    int gamepad = 0; // which gamepad to display
    Vector3 truckPosition = (Vector3){ 0.0f, 0.0f, 0.0f };
    Vector3 truckBedPosition = (Vector3){ 0.0f, 1.362f, 0.0f };
    Vector3 truckForward = { 0.0f, 0.0f, 1.0f };  // Forward is along +Z
    Vector3 rearAxleOffset = (Vector3){ 0, 0, -1.5f }; // adjust Z as needed
    Vector3 truckOrigin = (Vector3){0};
    float truckSpeed = 0.0f;
    float truckAngle = 0.0f; // Yaw angle
    float friction = 0.02f;
    //chase camera
    Vector3 cameraTargetPos = { 0 };
    Vector3 cameraOffset = { 0.0f, 6.0f, -14.0f };
    float camYaw = 0.0f;   // Left/right
    float camPitch = 15.0f; // Up/down, slightly above by default
    float camDistance = 14.0f;  // Distance from truck
    float relativeYaw = 0.0f;  // <-- instead of camYaw
    float relativePitch = 0.0f;  // <-- instead of camYaw
    //other gampepad stuff
    //SetConfigFlags(FLAG_MSAA_4X_HINT);  // Set MSAA 4X hint before windows creation
    // Set axis deadzones
    const float leftStickDeadzoneX = 0.1f;
    const float leftStickDeadzoneY = 0.1f;
    const float rightStickDeadzoneX = 0.1f;
    const float rightStickDeadzoneY = 0.1f;
    const float leftTriggerDeadzone = -0.9f;
    const float rightTriggerDeadzone = -0.9f;


    InitWindow(1280, 720, "Raylib Truck + Tires");
    SetTargetFPS(60);  
    DisableCursor();
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 8.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load truck
    Model truck = LoadModel("truck.obj");
    truck.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture("truck.png");

    // Load tire
    Model tire = LoadModel("tire.obj");
    tire.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture("tire.png");

    // Set tire offsets relative to truck
    Vector3 tireOffsets[4] = {
        {  1.6f, 0.0f,  3.36f }, // Front-right
        { -1.5f, 0.0f,  3.36f }, // Front-left
        {  1.6f, 0.0f, -2.64f }, // Rear-right
        { -1.6f, 0.0f, -2.64f }  // Rear-left
    };

    //SetCameraMode(camera, CAMERA_ORBITAL);

    while (!WindowShouldClose())
    {
        // Read from interrupt IN endpoint (0x84)
        unsigned char data[PACKET_SIZE];
        int actual_length = 0;
        ControllerData gpad = (ControllerData){0};
        bool readSuccess = false;
        r = libusb_interrupt_transfer(handle, 0x84, data, sizeof(data), &actual_length, 1000);
        if (r == 0) {
            printf("Read %d bytes:\n", actual_length);
            for (int i = 0; i < actual_length; i++) {
                printf("%02X ", data[i]);
            }
            printf("\n");
            gpad=ParseDualSenseInput(data, PACKET_SIZE);
            readSuccess = true;
        } else {
            printf("Read error or timeout: %s\n", libusb_error_name(r));
        }
        //update truck
        if (truckSpeed > 0.0f) {
            truckSpeed -= friction;
            if (truckSpeed < 0.0f) truckSpeed = 0.0f;  // Clamp to zero
        }
        else if (truckSpeed < 0.0f) {
            truckSpeed += friction;
            if (truckSpeed > 0.0f) truckSpeed = 0.0f;  // Clamp to zero
        }
        //handle input
        if (IsKeyDown(KEY_F12)) {TakeScreenshotWithTimestamp();}
        if (IsKeyDown(KEY_Y)) {contInvertY=!contInvertY;}
        if (IsKeyPressed(KEY_PAGE_UP)){pad_axis++;if(pad_axis>128){pad_axis=128;}}
        if (IsKeyPressed(KEY_PAGE_DOWN)){pad_axis--;if(pad_axis<0){pad_axis=0;}}
        if (IsKeyPressed(KEY_M))
        {
            if(mouse)
            {
                mouse=false;
                DisableCursor();
            }
            else
            {
                mouse=true;
                EnableCursor();
            }
        }
        float acceleration = 0.1f;
        float deceleration = 0.18f;
        float steeringSpeed = 1.5f;
        float maxSpeed = 3.0f;
        float maxSpeedReverse = -0.3f;
        float steerInput = 0;
        if (readSuccess)
        {
            
            // if (gpad.btnCross>0)//x - disabled for now
            // {
            //     truckSpeed += acceleration;
            // }
            // Deadzone
            if (fabsf(gpad.normLY) > 0.1f) {
                truckSpeed += -gpad.normLY * acceleration * GetFrameTime() * 64.0f;
                //printf("speed=%f",truckSpeed);
            }
            else {
                // Natural friction slowdown
                truckSpeed *= 0.95f; // or whatever damping feels good
            }
            if (gpad.btnSquare>0)//square
            {
                truckSpeed -= deceleration;
            }

            //some extra stuff for the truck - steering
            steerInput = (((float)gpad.lx - 128.0f) / 127.0f)/4 * GetFrameTime();
            float turnMax = 25.0f;
            if(steerInput>turnMax){steerInput=turnMax;}
            if(steerInput<-turnMax){steerInput=-turnMax;}
            truckAngle -= steerInput * steeringSpeed;
            //more steering - for the camera tho
            float sensitivity = 90.0f;  // degrees per second max
            float deadzone = 8.0f;

            float realRy = gpad.ry;
            if(contInvertY){realRy = 255 - realRy;}

            // if (fabsf(rxNorm) > deadzone / 127.0f) {camYaw += rxNorm * sensitivity * GetFrameTime();}
            // if (fabsf(ryNorm) > deadzone / 127.0f) {camPitch -= ryNorm * sensitivity * GetFrameTime();}

            if (fabsf(gpad.normRX) > deadzone / 127.0f) {
                relativeYaw += gpad.normRX * sensitivity * GetFrameTime();
            }
            if (fabsf(gpad.normRY) > deadzone / 127.0f) {
                relativePitch += -gpad.normRY * sensitivity * GetFrameTime();
            }

            // Clamp pitch so the camera doesn't go under or flip
            if (relativePitch < 5.0f) relativePitch = 5.0f;
            if (relativePitch > 85.0f) relativePitch = 85.0f;

            // Clamp speed
            if (truckSpeed > maxSpeed) {truckSpeed = maxSpeed;}
            if (truckSpeed < maxSpeedReverse) {truckSpeed = maxSpeedReverse;}
            //if (truckSpeed < -maxSpeed * 0.5f) truckSpeed = -maxSpeed * 0.5f; //do I need this?
            truckForward = (Vector3){ sinf(truckAngle), 0.0f, cosf(truckAngle) };
            truckPosition = Vector3Add(truckPosition, Vector3Scale(truckForward, truckSpeed));
            truckOrigin = Vector3Add(truckPosition, rearAxleOffset);
            //DrawText(TextFormat("axis ? %d", pad_axis), 10, 20, 10, GRAY);
            //DrawText(TextFormat("value? %f", axisValue), 10, 30, 10, GRAY);
        }
        //DrawText(TextFormat("GP%d [%s]", gamepad, GetGamepadName(gamepad)), 10, 10, 10, GRAY);

        //chase camera
        // Rotate that offset by the truck's angle to get world space
        // Vector3 rotatedOffset = {
        //     cameraOffset.x * cosf(truckAngle) - cameraOffset.z * sinf(truckAngle),
        //     cameraOffset.y,
        //     cameraOffset.x * sinf(truckAngle) + cameraOffset.z * cosf(truckAngle)
        // };

        // // Desired camera position is truckPosition + offset
        // Vector3 desiredCameraPos = Vector3Add(truckPosition, rotatedOffset);

        // // Smooth camera movement: interpolate toward desired position
        // float followSpeed = 5.0f * GetFrameTime();  // Tune this value
        // camera.position = Vector3Lerp(camera.position, desiredCameraPos, followSpeed);

        // // Always look at the truck (or slightly above it)
        // Vector3 desiredTarget = Vector3Add(truckPosition, (Vector3){ 0.0f, 2.0f, 0.0f });
        // camera.target = Vector3Lerp(camera.target, desiredTarget, followSpeed);
        camYaw = truckAngle * RAD2DEG + relativeYaw;
        //todo: remove camPitch if we can
        float radYaw = camYaw * DEG2RAD;
        float radPitch = relativePitch * DEG2RAD;
        float followSpeed = 5.0f * GetFrameTime();
        Vector3 offset = {
            camDistance * cosf(radPitch) * sinf(radYaw),
            camDistance * sinf(radPitch),
            camDistance * cosf(radPitch) * cosf(radYaw)
        };

        Vector3 desiredCameraPos = Vector3Add(truckPosition, offset);
        camera.position = Vector3Lerp(camera.position, desiredCameraPos, followSpeed);

        Vector3 desiredTarget = Vector3Add(truckPosition, (Vector3){ 0.0f, 2.0f, 0.0f });
        camera.target = Vector3Lerp(camera.target, desiredTarget, followSpeed);
        // if (relativePitch > 89.0f) {relativePitch = 89.0f;}
        // if (relativePitch < 10.0f) {relativePitch = 10.0f;}

        UpdateCamera(&camera,CAMERA_THIRD_PERSON);

        BeginDrawing();
        ClearBackground(RAYWHITE);
            BeginMode3D(camera);

            //Draw the truck
            //DrawModel(truck, Vector3Add(truckPosition, truckBedPosition), 4.8f, WHITE);
            DrawModelEx(truck, Vector3Add(truckOrigin, truckBedPosition), (Vector3){ 0, 1, 0 }, truckAngle * RAD2DEG, (Vector3){4.8f,4.8f,4.8f}, WHITE);
            //float frontTireSteerAngle = truckAngle * MAX_TURN_ANGLE; // e.g. 0.25 rad
            for (int i = 0; i < 4; i++)
            {
                // Vector3 localOffset = RotateY(tireOffsets[i], truckAngle);
                // Vector3 pos = Vector3Add(truckOrigin, localOffset);
                float tireAngle = truckAngle;  // Default for rear tires
                // Front tires (index 0 = front-right, 1 = front-left)
                if (i < 2) {//front tires
                    tireAngle += -PI/11.2f*gpad.normLX;
                }
                Vector3 localOffset = RotateY(tireOffsets[i], -truckAngle);
                Vector3 pos = Vector3Add(truckOrigin, localOffset);
                //DrawModel(tire, pos, 0.94f, WHITE);
                DrawModelEx(tire, pos, (Vector3){ 0, 1, 0 }, tireAngle * RAD2DEG, (Vector3){0.94f,0.94f,0.94f}, WHITE);
            }

            DrawGrid(20, 1.0f);
            EndMode3D();
        EndDrawing();
    }

    if (handle) libusb_close(handle);
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    UnloadModel(truck);
    UnloadModel(tire);
    CloseWindow();
    return 0;
}
