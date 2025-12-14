#include <iostream>
#include <vector>

#include "Ray.h"
#include "Angle.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>

#include "RayMarchingRender.h"
#include "Objects/Sphere.h"

using namespace std;

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    Ray camera = Ray((X)*4+Z*1, X*-1-Z*0.2);

    auto sphere1 = new Sphere({-1, 0, 0}, 1, sf::Color::Red);
    auto sphere2 = new Sphere({1, 0, 0}, 0.5, sf::Color::Blue);

    RayMarchingRender renderer = RayMarchingRender(480, 360, PI/2, (X*-2+Y-Z*5).normalized(),
        {sphere1, sphere2});
    auto &window = renderer.window;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
        renderer.renderFrame(camera);
        // house[0].rotate(Angle(-PI/40, Z));
        renderer.light.rotate(fromAngleAxis(-PI/10, Z));
        camera.setOrigin(camera.getOrigin().rotated(fromAngleAxis(-PI/40, Z)));
        camera.rotate(fromAngleAxis(-PI/40, Z));

        window.display();
        window.clear();
    }
    delete sphere1;
    delete sphere2;
    return 0;
}
