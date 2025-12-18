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
    std::vector<Object*> scene;
    
    // White floor plane at Z = 0 (ground level)
    scene.push_back(new Plane({0, 0, 0}, Z, sf::Color::Green));
    
    // A sphere on the floor to look at (at position Y=10, Z=1 for radius)
    scene.push_back(new Sphere({0, 10, 1}, 1.0, sf::Color::Red));
    

    // Sun-like light source (bright yellow sphere in the sky)
    scene.push_back(new Sphere({0, 20, 15}, 2.0, sf::Color(255, 255, 200)));

    // Light direction (pointing from sun position)
    Vector3 lightDir = (Vector3(0, -20, 15) - Vector3(0, 0, 2)).normalized();
    
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
    double lastMouseX = renderer.width / 2.0;
    double lastMouseY = renderer.height / 2.0;

    window.setMouseCursorVisible(false);
    window.setMouseCursorGrabbed(true);  // Lock mouse cursor inside window

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
            }
            else if (event->is<sf::Event::MouseMoved>())
            {
                const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();

                double mouseX = mouseMoved->position.x;
                double mouseY = mouseMoved->position.y;
                
                // Center of window
                double centerX = renderer.width / 2.0;
                double centerY = renderer.height / 2.0;

                if (firstMouseMove)
                {
                    lastMouseX = centerX;
                    lastMouseY = centerY;
                    firstMouseMove = false;
                    // Reset mouse to center
                    sf::Mouse::setPosition(sf::Vector2i(static_cast<int>(centerX), static_cast<int>(centerY)), window);
                    continue;
                }

                // Calculate delta from center (relative movement)
                double deltaX = mouseX - centerX;
                double deltaY = mouseY - centerY;

                // Reset mouse to center to keep it locked
                sf::Mouse::setPosition(sf::Vector2i(static_cast<int>(centerX), static_cast<int>(centerY)), window);

                // FPS-style yaw & pitch
                // Yaw rotates around Z axis (horizontal)
                // Pitch is vertical angle from horizontal
                yaw += deltaX * mouseSensitivity;
                pitch -= deltaY * mouseSensitivity;  // Negative for natural mouse up = look up

                // Clamp pitch to prevent flipping
                if (pitch > maxPitch) pitch = maxPitch;
                if (pitch < -maxPitch) pitch = -maxPitch;

                // Convert spherical coordinates to direction vector
                // Z is up, so:
                // - Yaw rotates around Z (horizontal plane is XY)
                // - Pitch is angle from horizontal
                // Standard FPS: yaw=0 looks along +Y, pitch=0 is horizontal
                Vector3 forward(
                    std::cos(pitch) * std::sin(yaw),  // X component (right)
                    std::cos(pitch) * std::cos(yaw),  // Y component (forward)
                    std::sin(pitch)                    // Z component (up)
                );

                camera.setDirection(forward.normalized());
            }
        }

        // Render
        renderer.renderFrame(camera);
        window.display();
        window.clear();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        std::cout << "FPS: " << 1000.0 / duration.count() << "\n";
    }

    for (auto* object : scene)
        delete object;

    return 0;
}
