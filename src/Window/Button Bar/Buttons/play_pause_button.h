#pragma once
#include "Window/Button Bar/SDL_Button.h"
namespace Window
{
    namespace Button
    {
        class PlayPauseButton : public SDLButton
        {
        public:
            PlayPauseButton( SDL_Texture* texture, const SDL_Rect& playRect, const SDL_Rect& pauseRect, const SDL_Rect& drawRect, bool& pauseMode );
            void clicked() override;
            void draw( SDL_Renderer* renderer ) override;
        private:
            bool& m_pauseMode;
            SDL_Rect m_pauseRect, m_playRect;
            SDL_Renderer* m_renderer = nullptr;
            SDL_Rect m_viewPort;
        };
    };
};
