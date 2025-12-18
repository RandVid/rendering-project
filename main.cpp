#include <iostream>
#include <vector>

#include "Ray.h"
#include "Angle.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>

#include "CSGoperations/Union.h"
#include "CSGoperations/Intersection.h"
#include "CSGoperations/Difference.h"

#include "RayMarchingRender.h"
#include "Objects/Plane.h"
#include "Objects/Sphere.h"
#include "Objects/Box.h"
#include "Objects/Cylinder.h"
#include "Objects/Capsule.h"
#include "Objects/Torus.h"
#include "Objects/Box.h"
#include "Objects/Cylinder.h"

using namespace std;

void addCSGShapes(std::vector<Object*>& scene) {
    // Example 1: Union of two spheres
    Object* sphere1 = new Sphere({-2, 0, 10}, 5, sf::Color::Red);
    Object* sphere2 = new Sphere({2, 0, 8}, 5, sf::Color::Blue);
    Object* unionObj = new Union(sphere1, sphere2);
    // scene.push_back(unionObj);
    Object* intersectObj = new Intersection(sphere1, sphere2);
    // scene.push_back(intersectObj);

    Object* differenceObj = new Difference(sphere1, sphere2);
    // scene.push_back(differenceObj);
}


int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    Ray camera = Ray((Z)*-50, Z*1-X*0.005);

    /*
    std::vector<Object*> scene = {
        new Sphere({-1, 0, 0}, 1, sf::Color::Red),
        new Sphere({1, 0, 0}, 0.5, sf::Color::Blue),
        new Sphere({0, 0, 1}, 0.5, sf::Color::Green),
        new Sphere({0, 0, -1}, 0.5, sf::Color::Yellow),
    };
    */

    /*
    for (int i = -30; i < 30; i++) {
        cout << i << endl;
        scene.push_back(new Sphere({static_cast<double>(i), 0, 0}, 0.5, sf::Color::White));
    }

    scene.push_back(new Sphere({static_cast<double>(-30), 0, 0}, 5, sf::Color::White));
    scene.push_back(new Sphere({static_cast<double>(30), 0, 0}, 5, sf::Color::White));
    */

    std::vector<Object*> scene;

    // Add some primitive spheres
    // scene.push_back(new Sphere({-1, 0, 0}, 1, sf::Color::Red));
    // scene.push_back(new Sphere({1, 0, 0}, 0.5, sf::Color::Blue));

    // Add CSG objects
    addCSGShapes(scene);

    // Add other shapes for testing
    // scene.push_back(new Plane({0, -10, 0}, Y, sf::Color::White));
    // scene.push_back(new Box({5, 0, 8}, {1,1,1}, sf::Color::Magenta));
    // scene.push_back(new Cylinder({-5, 0, 8}, 1, 3, sf::Color::Cyan));
    // scene.push_back(new Capsule({0, 4, 8}, {0, 6, 8}, 0.5, sf::Color::Green));
    // scene.push_back(new Torus({0, -5, 8}, 2, 0.5, sf::Color::Yellow));

    // scene.push_back(new Plane({0, 0, 0}, Z));


    RayMarchingRender renderer = RayMarchingRender(1280, 720, PI/2, (X*-2+Y-Z*5).normalized(), scene);
    auto &window = renderer.window;

    int count = 0;

    while (window.isOpen())
    {
        auto start = std::chrono::high_resolution_clock::now();
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::Resized>()) {
                if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                    unsigned newWidth = resized->size.x;
                    unsigned newHeight = resized->size.y;
                    renderer.setSize(newWidth, newHeight);
                }
            }
            else if (event->is<sf::Event::MouseMoved>()) {
            }
        }
        renderer.renderFrame(camera);
        // house[0].rotate(Angle(-PI/40, Z));
        // renderer.light.rotate(fromAngleAxis(-PI/10, Z));
        camera.getOrigin().rotate(fromAngleAxis(-PI/40, Z));
        camera.rotate(fromAngleAxis(-PI/400, Z));

        window.display();
        window.clear();
        auto end = std::chrono::high_resolution_clock::now();

        // Compute duration in milliseconds (you can change to microseconds, seconds, etc.)
        std::chrono::duration<double, std::milli> duration = end - start;

        std::cout << "Program ran for " << 1000 / duration.count() << " fps\n";
        // cout << count++ << endl;
    }
    for (const auto object : scene) {
        delete object;
    }
    return 0;
}
