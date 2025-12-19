#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <optional>
#include <set>
#include <random>
#include <string>

#include "Objects/Box.h"
#include "Ray.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>

#include "RayMarchingRender.h"
#include "Objects/Cylinder.h"
#include "Objects/Plane.h"
#include "Objects/Sphere.h"

using namespace std;

// ---------------- TELEPORT NODES (doubly-linked list) ----------------
struct TeleportNode {
    std::string name;
    Vector3 pos;
    Vector3 dir; // should be normalized-ish
    TeleportNode* prev = nullptr;
    TeleportNode* next = nullptr;
};

static TeleportNode* makeNode(std::string name, const Vector3& pos, const Vector3& dir) {
    auto* n = new TeleportNode();
    n->name = std::move(name);
    n->pos = pos;
    n->dir = dir.normalized();
    return n;
}

static void linkNodes(TeleportNode* a, TeleportNode* b) {
    if (a) a->next = b;
    if (b) b->prev = a;
}

static void deleteNodeChain(TeleportNode* head) {
    while (head) {
        TeleportNode* nxt = head->next;
        delete head;
        head = nxt;
    }
}

static void yawPitchFromDirZUp(const Vector3& dir, double& yaw, double& pitch) {
    Vector3 f = dir.normalized();
    // yaw=0 looks along +Y, yaw positive rotates toward +X
    yaw = std::atan2(f.getX(), f.getY());
    pitch = std::asin(std::max(-1.0, std::min(1.0, f.getZ())));
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Random number generator for QuaternionJulia animation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> signDist(0, 1);  // 0 or 1 for sign

    // ---------------- CAMERA ----------------
    // Start camera at standing height (Z = 2) looking forward
    // Z is up, Y is forward, X is right
    Ray camera({-9, 42, 10}, Y);

    // FPS camera state - using proper spherical coordinates
    // Yaw: rotation around Z axis (horizontal look left/right)
    // Pitch: angle from horizontal plane (vertical look up/down)
    double yaw = 0.0;      // 0 = looking along +Y
    double pitch = 0.0;    // 0 = looking horizontally
    const double mouseSensitivity = 0.003;
    const double maxPitch = PI / 2.1;

    // ---------------- TELEPORT CHAIN ----------------
    // Left/Right: move between nodes and teleport immediately.
    // Up: teleport to current node (no selection change).
    TeleportNode* tp0 = makeNode("n0", Vector3(-9, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp1 = makeNode("n1", Vector3(1, 42, 10), Vector3(0, 1,0));
    TeleportNode* tp2 = makeNode("n2", Vector3(11, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp3 = makeNode("n3", Vector3(21, 42, 10), Vector3(0, 1, 0));
    

    TeleportNode* tp4 = makeNode("n4", Vector3(31, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp5 = makeNode("n5", Vector3(41, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp6 = makeNode("n6", Vector3(51, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp7 = makeNode("n7", Vector3(61, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp8 = makeNode("n8", Vector3(71, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp9 = makeNode("n9", Vector3(81, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp10 = makeNode("n10", Vector3(91, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp11 = makeNode("n11", Vector3(101, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp12 = makeNode("n12", Vector3(111, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp13 = makeNode("n13", Vector3(121, 42, 10), Vector3(0, 1, 0));
    TeleportNode* tp14 = makeNode("n14", Vector3(131, 42, 10), Vector3(0, 1, 0));

    
    linkNodes(tp0, tp1);
    linkNodes(tp1, tp2);
    linkNodes(tp2, tp3);
    linkNodes(tp3, tp4);
    linkNodes(tp4, tp5);
    linkNodes(tp5, tp6);
    linkNodes(tp6, tp7);
    linkNodes(tp7, tp8);
    linkNodes(tp8, tp9);
    linkNodes(tp9, tp10);
    linkNodes(tp10, tp11);
    linkNodes(tp11, tp12);
    linkNodes(tp12, tp13);
    linkNodes(tp13, tp14);
    TeleportNode* selectedTeleport = tp0;

    // ---------------- SCENE ----------------
    // Shadow demonstration scene:
    // - Big box at the top
    // - Sphere below the box (should be in shadow, but isn't without shadow implementation)
    std::vector<Object*> scene;

    // Terrain: gentle hills around origin. originXZ = (0,0,0) -> we use x,z for horizontal domain, y stores seed
    // auto* terrain = new Terrain({0, 0, 0}, /*amplitude*/ 30.0f, /*frequency*/ 0.005f, /*seed*/ 3.0f, sf::Color(30, 140, 40));
    // auto* terrain2 = new Terrain({0, 0, -50}, /*amplitude*/ 80.0f, /*frequency*/ 0.005f, /*seed*/ 3.0f, sf::Color(30, 35, 40));
    // terrain->setWarp(2.0f, true).setRidged(false);
    // terrain2->setWarp(2.0f, true).setRidged(false);
    // scene.push_back(terrain);
    // scene.push_back(terrain2);
    // A sphere above terrain to look at
    // scene.push_back(new Sphere({0, 10, 6}, 1.0, sf::Color::Red));

    // Green floor plane at Z = 0 (ground level)
    scene.push_back(new Plane({0, 0, 0}, Z, sf::Color::Green, 0.2));
    // A sphere on the floor to look at (at position Y=10, Z=1 for radius)
    //scene.push_back(new Sphere({0, 10, 1}, 5.0, sf::Color::Red, "textures/petyb.jpg"));
    //scene.push_back(new Mandelbulb({20, 25, 10}, 8, 1.0, sf::Color::Red, 30, "textures/Texturelabs_Atmosphere_126M.jpg"));
    // scene.push_back(new Box({141, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide16.png"));
    // scene.push_back(new Box({131, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide15.png"));
    scene.push_back(new Box({121, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide14.png"));
    scene.push_back(new Box({111, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide13.png"));
    scene.push_back(new Box({101, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide12.png"));
    scene.push_back(new Box({91, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide11.png"));
    scene.push_back(new Box({81, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide10.png"));
    scene.push_back(new Box({71, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide9.png"));
    scene.push_back(new Box({61, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide8.png"));
    scene.push_back(new Box({51, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide7.png"));
    scene.push_back(new Box({41, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide6.png"));
    scene.push_back(new Box({31, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide5.png"));
    scene.push_back(new Box({21, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide4.png"));
    scene.push_back(new Box({11, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide3.png"));
    scene.push_back(new Box({1, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide2.png"));
    scene.push_back(new Box({-9, 50, 10}, {5, 3, 3}, sf::Color::Blue, "textures/slide1.png"));

    //scene.push_back(new Mandelbulb({0, 5, 50}, 8, 1.0, sf::Color::Red, 30, "textures/fire.jpg"));
    scene.push_back(new Box({1, 1, 1}, {1, 2, 1}, sf::Color::Blue, "textures/petyb.jpg"));
    // scene.push_back(new Box({5, 1, 1}, {1, 1, 1}, sf::Color::Blue, "textures/Pavel.png"));
    // scene.push_back(new Box({9, 1, 1}, {1, 1, 1}, sf::Color::Blue, "textures/Anatoly.png"));
    // scene.push_back(new QuaternionJulia(
    //     {0, 5, 30},           // center position
    //     {0.3, 0.5, 0.1},     // Julia constant c (affects the fractal shape)
    //     12,                   // iterations (more = more detail)
    //     20.0,                  // scale
    //     sf::Color::Magenta,   // color
    //     "textures/fire.jpg"  // optional texture
    // ));

    // Big box floating above (at Z = 8, centered at Y = 10)
    // This box should cast a shadow on the sphere below
    scene.push_back(new Box({15, 10, 8}, {2.0, 2.0, 1.0}, sf::Color::White, 0.5));

    // // Optional: keep fractal far away
    // scene.push_back(new Mandelbulb({0, 5, 50}, 8, 1.0, sf::Color::Red, 40));
    // Sphere below the box (at Y = 10, Z = 2)
    // Give it some reflectivity so reflections are visible (0 = none, 1 = mirror)
    scene.push_back(new Sphere({20, 20, 2}, 1.5, sf::Color::White, 0.8f));

    // scene.push_back(new Box({1, 1, 1}, {1, 1, 1}, sf::Color::Blue, "textures/Texturelabs_Atmosphere_126M.jpg"));
    // Sun-like light source (bright yellow sphere in the sky)
    scene.push_back(new Cylinder({-30, 20, 15}, 2.0, 3.0, sf::Color(255, 255, 200)));
    // scene.push_back(new Torus({30, 20, 40}, 2.0, 3.0, sf::Color(255, 255, 200)));

    // Light direction (pointing from sun position)
    // Light source positioned above and to the side
    // This creates a clear shadow that should fall on the sphere
    //scene.push_back(new Sphere({-5, 10, 12}, 1.0, sf::Color::White));

    // Light direction (pointing from light position toward the scene)
    Vector3 lightDir = (Vector3(0, -20, 15) - Vector3(0, 0, 2)).normalized();
    // scene.push_back(new Sphere({0, -21, 16}, 0.2, sf::Color::Yellow));
    RayMarchingRender renderer(
        1200,
        720,
        PI / 3,  // 60 degrees FOV - more natural, less warping
        lightDir,
        scene
    );

    auto& window = renderer.window;

    // Mouse control state
    bool mouseInWindow = false;
    bool firstMouseMove = true;
    bool lmbDown = false;
    double lastMouseX = renderer.width / 2.0;
    double lastMouseY = renderer.height / 2.0;
    double moveSpeed = 10;

    window.setMouseCursorVisible(true);  // Show mouse cursor

    // Track pressed keys for event-based input (avoids permission issues)
    std::set<sf::Keyboard::Key> pressedKeys;

    auto teleportToSelected = [&]() {
        if (!selectedTeleport) return;
        camera.setOrigin(selectedTeleport->pos);
        camera.setDirection(selectedTeleport->dir.normalized());
        yawPitchFromDirZUp(selectedTeleport->dir, yaw, pitch);
        // prevent mouse delta jump after teleport
        firstMouseMove = true;
        std::cout << "[Teleport] moved to: " << selectedTeleport->name << "\n";
    };

    double fps = 1;
    // ---------------- MAIN LOOP ----------------
    while (window.isOpen())
    {
        auto start = std::chrono::high_resolution_clock::now();

        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::Resized>())
            {
                const auto* resized = event->getIf<sf::Event::Resized>();
                renderer.setSize(resized->size.x, resized->size.y);
                // Update center position after resize
                lastMouseX = resized->size.x / 2.0;
                lastMouseY = resized->size.y / 2.0;
                firstMouseMove = true;
            }
            else if (event->is<sf::Event::MouseEntered>())
            {
                mouseInWindow = true;
                firstMouseMove = true;
            }
            else if (event->is<sf::Event::MouseLeft>())
            {
                mouseInWindow = false;
                lmbDown = false;
            }
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                const auto* mb = event->getIf<sf::Event::MouseButtonPressed>();
                if (mb->button == sf::Mouse::Button::Left)
                {
                    lmbDown = true;
                    firstMouseMove = true;   // чтобы не дёргалось при первом движении
                }
            }
            else if (event->is<sf::Event::MouseButtonReleased>())
            {
                const auto* mb = event->getIf<sf::Event::MouseButtonReleased>();
                if (mb->button == sf::Mouse::Button::Left)
                {
                    lmbDown = false;
                    firstMouseMove = true;
                }
            }
            else if (event->is<sf::Event::MouseMoved>())
            {
                // if (!mouseInWindow || !lmbDown)
                //     continue;

                if (!lmbDown)
                    continue;

                const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();

                double mouseX = mouseMoved->position.x;
                double mouseY = mouseMoved->position.y;

                if (firstMouseMove)
                {
                    lastMouseX = mouseX;
                    lastMouseY = mouseY;
                    firstMouseMove = false;
                    continue;
                }

                double deltaX = mouseX - lastMouseX;
                double deltaY = mouseY - lastMouseY;

                lastMouseX = mouseX;
                lastMouseY = mouseY;

                yaw += deltaX * mouseSensitivity;
                pitch -= deltaY * mouseSensitivity;

                if (pitch > maxPitch) pitch = maxPitch;
                if (pitch < -maxPitch) pitch = -maxPitch;

                Vector3 forward(
                    std::cos(pitch) * std::sin(yaw),
                    std::cos(pitch) * std::cos(yaw),
                    std::sin(pitch)
                );

                camera.setDirection(forward.normalized());
            }
            else if (event->is<sf::Event::KeyPressed>())
            {
                const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                pressedKeys.insert(keyPressed->code);

                // Teleport node navigation (single-step, on press)
                if (keyPressed->code == sf::Keyboard::Key::Left) {
                    if (selectedTeleport && selectedTeleport->prev) {
                        selectedTeleport = selectedTeleport->prev;
                        std::cout << "[Teleport] selected: " << selectedTeleport->name << "\n";
                        teleportToSelected();
                    }
                } else if (keyPressed->code == sf::Keyboard::Key::Right) {
                    if (selectedTeleport && selectedTeleport->next) {
                        selectedTeleport = selectedTeleport->next;
                        std::cout << "[Teleport] selected: " << selectedTeleport->name << "\n";
                        teleportToSelected();
                    }
                } else if (keyPressed->code == sf::Keyboard::Key::Up) {
                    teleportToSelected();
                }
            }
            else if (event->is<sf::Event::KeyReleased>())
            {
                const auto* keyReleased = event->getIf<sf::Event::KeyReleased>();
                pressedKeys.erase(keyReleased->code);
            }
        }

        // WASD Movement - check keyboard state
        Vector3 moveDirection(0, 0, 0);

        // Get current camera direction
        Vector3 forward = camera.getDirection().normalized();

        // Calculate right vector (for strafing)
        // Right = forward cross world up (Z)
        Vector3 right = forward.cross(Z);
        if (right.magnitude() < 0.001) {
            right = X;  // Fallback if forward is parallel to Z
        } else {
            right = right.normalized();
        }

        // Calculate forward movement (project forward onto horizontal plane, remove Z component)
        Vector3 forwardHorizontal(forward.getX(), forward.getY(), 0);
        if (forwardHorizontal.magnitude() < 0.001) {
            // If looking straight up/down, choose a stable "forward" direction on the ground plane
            forwardHorizontal = Y;
        } else {
            forwardHorizontal = forwardHorizontal.normalized();
        }

        // W - Move forward
        if (pressedKeys.contains(sf::Keyboard::Key::W)) {
            moveDirection = moveDirection + forwardHorizontal;
        }

        // S - Move backward
        if (pressedKeys.contains(sf::Keyboard::Key::S)) {
            moveDirection = moveDirection - forwardHorizontal;
        }
        
        // A - Strafe left
        if (pressedKeys.contains(sf::Keyboard::Key::A)) {
            moveDirection = moveDirection - right;
        }

        // D - Strafe right
        if (pressedKeys.contains(sf::Keyboard::Key::D)) {
            moveDirection = moveDirection + right;
        }

        // Q - Move up (positive Z)
        if (pressedKeys.contains(sf::Keyboard::Key::Q)) {
            moveDirection = moveDirection - Z;
        }

        // E - Move down (negative Z)
        if (pressedKeys.contains(sf::Keyboard::Key::E)) {
            moveDirection = moveDirection + Z;
        }

        if (pressedKeys.contains(sf::Keyboard::Key::LShift)) {
            moveSpeed *= pow(2, 1/fps);
        }

        if (pressedKeys.contains(sf::Keyboard::Key::LControl)) {
            moveSpeed /= pow(2, 1/fps);
            if (moveSpeed < 0.01) moveSpeed = 0.01;
        }

        cout << moveSpeed << endl;

        // Apply movement if any key is pressed
        if (moveDirection.magnitude() > 0.001) {
            moveDirection = moveDirection.normalized() * moveSpeed / fps;
            camera.move(moveDirection);
        }

        // Render
        renderer.renderFrame(camera);
        window.display();
        window.clear();

        // if (dynamic_cast<Mandelbulb*>(scene[2]))
        //     dynamic_cast<Mandelbulb*>(scene[2])->power += 0.5 / fps;

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "FPS: " << 1000.0 / duration.count() << "\n";
        fps = 1000.0 / duration.count();
    }

    for (auto* object : scene)
        delete object;

    deleteNodeChain(tp0);

    return 0;
}