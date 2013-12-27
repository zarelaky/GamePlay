// Implementation of base platform-agnostic platform functionality.
#include "Base.h"
#include "Platform.h"
#include "Game.h"
#include "ScriptController.h"
#include "Form.h"

namespace gameplay
{

void Platform::touchEventInternal(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex, bool actuallyMouse)
{
    if (actuallyMouse || !Form::touchEventInternal(evt, x, y, contactIndex))
    {
        Game::getInstance()->touchEvent(evt, x, y, contactIndex);
        Game::getInstance()->getScriptController()->touchEvent(evt, x, y, contactIndex);
    }
}

void Platform::keyEventInternal(Keyboard::KeyEvent evt, int key)
{
    if (!Form::keyEventInternal(evt, key))
    {
        Game::getInstance()->keyEvent(evt, key);
        Game::getInstance()->getScriptController()->keyEvent(evt, key);
    }
}

bool Platform::mouseEventInternal(Mouse::MouseEvent evt, int x, int y, int wheelDelta)
{
    if (Form::mouseEventInternal(evt, x, y, wheelDelta))
    {
        return true;
    }
    else if (Game::getInstance()->mouseEvent(evt, x, y, wheelDelta))
    {
        return true;
    }
    else
    {
        return Game::getInstance()->getScriptController()->mouseEvent(evt, x, y, wheelDelta);
    }
}

void Platform::gestureSwipeEventInternal(int x, int y, int direction)
{
    // TODO: Add support to Form for gestures
    Game::getInstance()->gestureSwipeEvent(x, y, direction);
    Game::getInstance()->getScriptController()->gestureSwipeEvent(x, y, direction);
}

void Platform::gesturePinchEventInternal(int x, int y, float scale)
{
    // TODO: Add support to Form for gestures
    Game::getInstance()->gesturePinchEvent(x, y, scale);
    Game::getInstance()->getScriptController()->gesturePinchEvent(x, y, scale);
}

void Platform::gestureTapEventInternal(int x, int y)
{
    // TODO: Add support to Form for gestures
    Game::getInstance()->gestureTapEvent(x, y);
    Game::getInstance()->getScriptController()->gestureTapEvent(x, y);
}

void Platform::gestureLongTapEventInternal(int x, int y, float duration)
{
    // TODO: Add support to Form for gestures
	Game::getInstance()->gestureLongTapEvent(x, y, duration);
	Game::getInstance()->getScriptController()->gestureLongTapEvent(x, y, duration);
}

void Platform::gestureDragEventInternal(int x, int y)
{
    // TODO: Add support to Form for gestures
	Game::getInstance()->gestureDragEvent(x, y);
	Game::getInstance()->getScriptController()->gestureDragEvent(x, y);
}

void Platform::gestureDropEventInternal(int x, int y)
{
    // TODO: Add support to Form for gestures
	Game::getInstance()->gestureDropEvent(x, y);
	Game::getInstance()->getScriptController()->gestureDropEvent(x, y);
}

void Platform::resizeEventInternal(unsigned int width, unsigned int height)
{
    // Update the width and height of the game
    Game* game = Game::getInstance();
    if (game->_width != width || game->_height != height)
    {
        game->_width = width;
        game->_height = height;
        game->resizeEvent(width, height);
        game->getScriptController()->resizeEvent(width, height);
    }

    Form::resizeEventInternal(width, height);
}

void Platform::gamepadEventInternal(Gamepad::GamepadEvent evt, Gamepad* gamepad, unsigned int analogIndex)
{
    if (!Form::gamepadEventInternal(evt, gamepad, analogIndex))
    {
        Game::getInstance()->gamepadEvent(evt, gamepad);
        Game::getInstance()->getScriptController()->gamepadEvent(evt, gamepad);
    }
}

void Platform::gamepadEventConnectedInternal(GamepadHandle handle,  unsigned int buttonCount, unsigned int joystickCount, unsigned int triggerCount,
                                             unsigned int vendorId, unsigned int productId, const char* vendorString, const char* productString)
{
    Gamepad::add(handle, buttonCount, joystickCount, triggerCount, vendorId, productId, vendorString, productString);
}

void Platform::gamepadEventDisconnectedInternal(GamepadHandle handle)
{
    Gamepad::remove(handle);
}

}
