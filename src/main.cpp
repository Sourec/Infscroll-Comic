#include "Hydra.h"
#include "Sprite.h"
#include <vector>
#include <unistd.h>
#include <stdio.h>
using namespace std;
using namespace Hydra;

enum dirs {left, right, up, down};

class ComicPanel
{
public:
    ComicPanel();
    int posX;
    int posY;
    int height;
    int width;
    float vel; //Speed when traveling TO this panel!    bool root;
    bool blank; //If true, don't render this panel - it's just an empty frame.
    bool root; //Is this comic a root?
    bool autopos; //If true, this comic's position will be determined dynamically
    bool autoprocessed; //If true, this comic has already been processed by the recursive positioning algorithm.
    string nextComic[4]; //The name of the next panel to be moved to given
    string name;
    Sprite* image;
};

ComicPanel* loadFromXML(pugi::xml_node configNode);
ComicPanel* switchToComic(vector<ComicPanel*>* panels, string nextName);
void performAutoPos(ComicPanel* lastPanel, ComicPanel* nextPanel, dirs dir);
void recursivePositioning(ComicPanel* panel, vector<ComicPanel*>* panels);
inline double getScaling(ComicPanel* panel, HydraEngine* engine);

Log* sysLog = Logger::getInstance()->getLog("sysLog");

int main(int argc, char* argv[])
{
	HydraEngine* engine = HydraEngine::getInstance();
	engine->init();
	engine->setWTitle("InfScroll-Comic");
	srand(time(0));

    vector<ComicPanel*> panels;
    bool quit = false;

    double currentX = 0, currentY = 0;
    double dX = 0, dY = 0;
    double scale = 1.0f;
    double scalingDiff = 0.0f; //How much to change the scaling by
    bool transitioning = false;
    bool fullscreen = false;
    int fcCooldown = 0;
    ComicPanel* currentPanel = nullptr;

    //Load all comics
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("Comic.xml");

    sysLog->log("Loaded file Comic.xml with result: " + string(result.description()));
    sysLog->log("Loading panels...", Hydra::resource);

    for (pugi::xml_node comic = doc.child("Comics").child("panel"); comic; comic = comic.next_sibling())
    {
        ComicPanel* newPanel = loadFromXML(comic);
        if (newPanel->root)
        {
            currentPanel = newPanel;
            scale = getScaling(currentPanel, engine); //The comic is to start out scaled to the root panel
            currentX = currentPanel->posX;
            currentY = currentPanel->posY;
        }
        if (newPanel->name == "rickastley")
            engine->setWTitle("InfScroll-Comic " + engine->getVNumber() + " - This comic is Astley-enabled!");
        panels.push_back(newPanel);
    }

    if (currentPanel == nullptr)
        sysLog->log("Error - couldn't find the root panel! Have you specified one yet?", Hydra::error);

    //Try to recursively solve the panel positions
    for (auto iter = panels.begin(); iter != panels.end(); iter++)
        recursivePositioning(*iter, &panels);

    while (!quit && currentPanel != nullptr)
    {
        if (fcCooldown > 0)
            fcCooldown--;
        Timer fpsTimer;
        fpsTimer.start();
        fpsTimer.setInterval(1000.0f / 60.0f);

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT || e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                quit = true;
            if (e.key.keysym.scancode == SDL_SCANCODE_F && !transitioning && fcCooldown == 0)
            {
                if (!fullscreen)
                {
                    fullscreen = true;
                    SDL_DisplayMode vmode;
                    SDL_GetCurrentDisplayMode(0, &vmode);
                    engine->setWSize(vmode.w, vmode.h);
                    SDL_SetWindowFullscreen(engine->getWindow(), SDL_WINDOW_FULLSCREEN);
                }
                else
                {
                    fullscreen = false;
                    SDL_SetWindowFullscreen(engine->getWindow(), 0);
                    engine->setWSize(800, 600);
                }

                scale = getScaling(currentPanel, engine);
                fcCooldown = 30;
            }
        }

        //Transitioning stuff
        if (!transitioning)
        {
            ComicPanel* nextPanel = nullptr;

            const Uint8* keystate = SDL_GetKeyboardState(nullptr);
            if (keystate[SDL_SCANCODE_DOWN])
                nextPanel = switchToComic(&panels, currentPanel->nextComic[dirs::down]);
            if (keystate[SDL_SCANCODE_UP])
                nextPanel = switchToComic(&panels, currentPanel->nextComic[dirs::up]);
            if (keystate[SDL_SCANCODE_LEFT])
                nextPanel = switchToComic(&panels, currentPanel->nextComic[dirs::left]);
            if (keystate[SDL_SCANCODE_RIGHT])
                nextPanel = switchToComic(&panels, currentPanel->nextComic[dirs::right]);

            if (nextPanel != nullptr)
            {
                currentPanel = nextPanel;
                transitioning = true;
                sysLog->log("Switching panels to panel " + currentPanel->name);

                //Compute dX and dY values
                Vector2D dir;
                dir.rotate(atan2f((double)(currentPanel->posY - currentY), (double)(currentPanel->posX - currentX)));
                dir.setMag(currentPanel->vel);
                dX = dir.getX();
                dY = dir.getY();
            }
        }

        //Velocities in directions - uses a Vector2D for calculations (because TRIGONOMETRY!)
        if (transitioning)
        {
            Vector2D dir;
            dir.setX(abs((int)(scale * ((double)currentPanel->posX - currentX))));
            dir.setY(abs((int)(scale * ((double)currentPanel->posY - currentY))));
            scalingDiff = (getScaling(currentPanel, engine) - scale) / (dir.getMag() / (double)currentPanel->vel);

            scale += scalingDiff;
            currentX += dX;// * scale;
            currentY += dY;// * scale;

            if (dir.getMag() <= currentPanel->vel)
            {
                transitioning = false;
                currentX = (double)currentPanel->posX; //Absolute magic. Complete effing magic!
                currentY = (double)currentPanel->posY;
                scale= getScaling(currentPanel, engine); //Fix scaling

                sysLog->log("Viewer at " + to_string(currentX) + ", " + to_string(currentY) + ". Scale: " + to_string(getScaling(currentPanel, engine)));
            }
        }

        //Rendering stuff
        SDL_RenderClear(engine->getRenderer());
        for (auto iter = panels.begin(); iter != panels.end(); iter++)
        {
            if ((*iter)->blank)
              continue;
            Sprite* image = (*iter)->image;
            if ((*iter)->name == "rickastley")
            {
                switch (rand() % 3)
                {
                    case 0:
                        image->setR(rand() % 256);
                        break;
                    case 1:
                        image->setG(rand() % 256);
                        break;
                    case 2:
                        image->setB(rand() % 256);
                        break;
                }
            }
            image->render(((*iter)->posX - currentX) * scale,
                          ((*iter)->posY - currentY) * scale,
                          image->getH() * scale,
                          image->getW() * scale);
        }

        SDL_RenderPresent(engine->getRenderer());

        //Ensures 60 fps cap
        while (!fpsTimer.hasIntervalPassed());
    }

    //Shutdown sequence
    sysLog->log("Freeing images...", Hydra::resource);
    for (auto iter = panels.begin(); iter != panels.end(); iter++)
    {
        sysLog->log("Freeing " + (*iter)->name + "...", Hydra::resource);
        (*iter)->image->free();
        delete *iter;
    }
    engine->shutdown();
}
ComicPanel::ComicPanel()
{
    autoprocessed = false;
}
ComicPanel* loadFromXML(pugi::xml_node configNode)
{
    //Assumes that this node is a "panel" node.
    ComicPanel* newPanel = new ComicPanel;
    newPanel->image = new Sprite;

    newPanel->name = configNode.attribute("name").as_string();
    newPanel->posX = configNode.child("position").attribute("x").as_int();
    newPanel->posY = configNode.child("position").attribute("y").as_int();
    newPanel->vel = configNode.child("vel").attribute("value").as_float();
    newPanel->root = false; //Default value
    newPanel->root = configNode.child("root").attribute("enabled").as_bool();
    newPanel->blank = false; //Default value
    newPanel->blank = configNode.child("blank").attribute("enabled").as_bool();
    newPanel->autopos = false;
    newPanel->autopos = configNode.attribute("autopos").as_bool();
    if (newPanel->autopos)
        newPanel->blank = true; //Don't render until this thing can be positioned.
    for (int i = 0; i < 4; i++)
        newPanel->nextComic[i] = "null"; //Signifies that there is no transition to a new comic. These are default values.

    newPanel->nextComic[dirs::up] =       configNode.child("dirs").child("up").attribute("nextComic").as_string();
    newPanel->nextComic[dirs::down] =     configNode.child("dirs").child("down").attribute("nextComic").as_string();
    newPanel->nextComic[dirs::left] =     configNode.child("dirs").child("left").attribute("nextComic").as_string();
    newPanel->nextComic[dirs::right] =    configNode.child("dirs").child("right").attribute("nextComic").as_string();

    sysLog->log("Loaded panel " + newPanel->name, Hydra::resource);
    if (newPanel->blank && !newPanel->autopos)
    {
        newPanel->width = configNode.child("dims").attribute("width").as_int();
        newPanel->height = configNode.child("dims").attribute("height").as_int();
        return newPanel; //There is no image to load; skip loading an image.
    }

    newPanel->image->loadFromFile(configNode.child("filename").attribute("str").as_string());
    sysLog->log("Loaded file " + string(configNode.child("filename").attribute("str").as_string()), Hydra::resource);
    newPanel->height = newPanel->image->getH();
    newPanel->width = newPanel->image->getW();
    return newPanel;
}
ComicPanel* switchToComic(vector<ComicPanel*>* panels, string nextName)
{
    //Given a name of a panel and a vector of pointers to panels, find a panel with the given name.
    //Returns nullptr if there is no such pointer

    //Dummy check - XML file should specify null if you don't want to switch to comic from a dir
    if (nextName == "null")
        return nullptr;

    for (auto iter = panels->begin(); iter != panels->end(); iter++)
        if ((*iter)->name == nextName)
            return *iter;
    return nullptr;
}
void performAutoPos(ComicPanel* lastPanel, ComicPanel* nextPanel, dirs dir)
{
    //Figure out this thing's position using the position of the last panel
    if (dir == dirs::up)
    {
        nextPanel->posX = lastPanel->posX;
        nextPanel->posY = lastPanel->posY - nextPanel->height;
    }
    if (dir == dirs::down)
    {
        nextPanel->posX = lastPanel->posX;
        nextPanel->posY = lastPanel->posY + lastPanel->height;
    }
    if (dir == dirs::left)
    {
        nextPanel->posX = lastPanel->posX - nextPanel->width;
        nextPanel->posY = lastPanel->posY;
    }
    if (dir == dirs::right)
    {
        nextPanel->posX = lastPanel->posX + lastPanel->width;
        nextPanel->posY = lastPanel->posY;
    }

    nextPanel->autopos = false;
    nextPanel->blank = false;
}
void recursivePositioning(ComicPanel* panel, vector<ComicPanel*>* panels)
{
    if (panel == nullptr || panels == nullptr || panel->autopos || panel->autoprocessed)
        return;

    //Accepts a single panel to begin recursive operations on.
    for (int i = 0; i < 4; ++i)
    {
        ComicPanel* nextPanel = switchToComic(panels, panel->nextComic[i]);
        if (nextPanel == nullptr)
            continue;


        if (nextPanel->autopos)
            performAutoPos(panel, nextPanel, (dirs)i);

        sysLog->log("Attempting to perform recursive positioning on panel " + nextPanel->name);
        panel->autoprocessed = true;
        recursivePositioning(nextPanel, panels);
    }
}
double getScaling(ComicPanel* panel, HydraEngine* engine)
{
    //Figure out the scaling needed to fit this panel's largest dimension on the screen
    if (panel == nullptr || engine == nullptr)
        return 1.0f; //Can't do anything with a bunch of nulls, can I?

    double zWidth = (double)engine->getWXSize() / (double)panel->width;
    double zHeight = (double)engine->getWYSize() / (double)panel->height;

    if (zWidth < zHeight)
        return zWidth;
    else
        return zHeight;
}
