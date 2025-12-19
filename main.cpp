#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <optional>

#include "Ray.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>

#include "RayMarchingRender.h"
#include "Objects/Plane.h"
#include "Objects/Sphere.h"
#include "Objects/Box.h"

using namespace std;

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ---------------- CAMERA ----------------
    // Start camera at standing height (Z = 2) looking forward
    // Z is up, Y is forward, X is right
    Ray camera({0, 0, 2}, Y);

    // FPS camera state - using proper spherical coordinates
    // Yaw: rotation around Z axis (horizontal look left/right)
    // Pitch: angle from horizontal plane (vertical look up/down)
    double yaw = 0.0;      // 0 = looking along +Y
    double pitch = 0.0;    // 0 = looking horizontally
    const double mouseSensitivity = 0.003;
    const double maxPitch = PI / 2.1;

    // ---------------- SCENE ----------------
    // Shadow demonstration scene:
    // - Big box at the top
    // - Sphere below the box (should be in shadow, but isn't without shadow implementation)
    std::vector<Object*> scene;

    // Green floor plane at Z = 0 (ground level)
    scene.push_back(new Plane({0, 0, 0}, Z, sf::Color::Green, 0.5f));

    // Big box floating above (at Z = 8, centered at Y = 10)
    // This box should cast a shadow on the sphere below
    scene.push_back(new Box({0, 10, 8}, {2.0, 2.0, 1.0}, sf::Color::White, 1.0f));

    // Sphere below the box (at Y = 10, Z = 2)
    // Give it some reflectivity so reflections are visible (0 = none, 1 = mirror)
    scene.push_back(new Sphere({0, 20, 2}, 1.5, sf::Color::White, 0.0f));

    // Light source positioned above and to the side
    // This creates a clear shadow that should fall on the sphere
    scene.push_back(new Sphere({-5, 10, 12}, 1.0, sf::Color::White));

    // Light direction (pointing from light position toward the scene)
    Vector3 lightDir = (Vector3(0, -20, 15) - Vector3(0, 0, 2)).normalized();
    scene.push_back(new Sphere({0, -21, 16}, 0.2, sf::Color::Yellow));
    RayMarchingRender renderer(
        1280,
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

    // Movement settings
    const double moveSpeed = 0.1;  // Movement speed per frame

    window.setMouseCursorVisible(true);  // Show mouse cursor

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
        }

        // WASD Movement - check keyboard state
        Vector3 moveDirection(0, 0, 0);
        double moveSpeed = 10/fps;

        // Get current camera direction
        Vector3 forward = camera.getDirection().normalized();

        // Calculate right vector (for strafing)
        // Right = forward cross world up (Z)
        Vector3 right = forward.cross(Z).normalized();
        if (right.magnitude() < 0.001) {
            right = X;  // Fallback if forward is parallel to Z
        }
        right = right.normalized();

        // Calculate forward movement (project forward onto horizontal plane, remove Z component)
        Vector3 forwardHorizontal = Vector3(forward.getX(), forward.getY(), 0).normalized();

        // W - Move forward
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            moveDirection = moveDirection + forwardHorizontal;
        }

        // S - Move backward
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            moveDirection = moveDirection - forwardHorizontal;
        }
        
        // A - Strafe left
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            moveDirection = moveDirection - right;
        }

        // D - Strafe right
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            moveDirection = moveDirection + right;
        }

        // Q - Move up (positive Z)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) {
            moveDirection = moveDirection - Z;
        }

        // E - Move down (negative Z)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
            moveDirection = moveDirection + Z;
        }

        // Apply movement if any key is pressed
        if (moveDirection.magnitude() > 0.001) {
            moveDirection = moveDirection.normalized() * moveSpeed;
            camera.move(moveDirection);
        }

        // Render
        renderer.renderFrame(camera);
        window.display();
        window.clear();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "FPS: " << 1000.0 / duration.count() << "\n";
        fps = 1000.0 / duration.count();
    }

    for (auto* object : scene)
        delete object;

    return 0;
}