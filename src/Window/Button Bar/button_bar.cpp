#include "button_bar.h"

#include <SDL2/SDL_image.h> 

#include "Buttons/play_pause_button.h"
#include "Buttons/stop_button.h"
#include "Buttons/rewind_back_button.h"
#include "Buttons/rewind_forward_button.h"

namespace Window
{
    ButtonBar::ButtonBar( SDL_Renderer* renderer, SDL_Rect& rect ) : m_buttonBarRect( rect )
    {
        m_renderer = renderer;
        m_buttonsTexture = IMG_LoadTexture( m_renderer, m_filename );
        int w, h;
        SDL_QueryTexture( m_buttonsTexture, NULL, NULL, &w, &h );
        int buttonWidth = 0.7 * m_buttonBarRect.h;
        int buttonIndent = 0.15 * m_buttonBarRect.h;
        m_buttons.push_back( std::make_unique<Button::PlayPauseButton>(
            m_buttonsTexture,
            SDL_Rect( { 0, 0, h / 2, h / 2 } ),
            SDL_Rect( { 350, 0, h / 2, h / 2 } ),
            SDL_Rect( { m_buttonBarRect.w / 2 - buttonWidth, buttonIndent, buttonWidth, buttonWidth } ),
            m_pauseMode ) );
        m_buttons.push_back( std::make_unique<Button::StopButton>(
            m_buttonsTexture,
            SDL_Rect( { 700, 0, h / 2, h / 2 } ),
            SDL_Rect( { m_buttonBarRect.w / 2, buttonIndent, buttonWidth, buttonWidth } ) ) );
        m_buttons.push_back( std::make_unique<Button::RewindBackButton>(
            m_buttonsTexture,
            SDL_Rect( { 1050, 0, h / 2, h / 2 } ),
            SDL_Rect( { m_buttonBarRect.w / 2 - buttonWidth*2, buttonIndent, buttonWidth, buttonWidth } ) ) );
        m_buttons.push_back( std::make_unique<Button::RewindForwardButton>(
            m_buttonsTexture,
            SDL_Rect( { 1400, 0, h / 2, h / 2 } ),
            SDL_Rect( { m_buttonBarRect.w / 2 + buttonWidth, buttonIndent, buttonWidth, buttonWidth } ) ) );
    }

    ButtonBar::~ButtonBar()
    {
        SDL_DestroyTexture( m_buttonsTexture );
    }

    void ButtonBar::draw()
    {
        SDL_RenderSetViewport( m_renderer, &m_buttonBarRect );
        SDL_SetRenderDrawColor( m_renderer, 255, 255, 255, 255 );
        SDL_RenderFillRect( m_renderer, NULL );
        for( auto& button : m_buttons )
        {
            button->draw( m_renderer );
        }
        SDL_RenderPresent( m_renderer );
    }

    void ButtonBar::update( SDL_Point point )
    {
        if( SDL_PointInRect( &point, &m_buttonBarRect ) )
        {
            point.y -= m_buttonBarRect.y;
            for( auto& button : m_buttons )
                button->update( point );
        }
    }

    void ButtonBar::clicked()
    {
        for( auto& button : m_buttons )
        {
            if( button->selected() )
                button->clicked();
        }
    }

    void ButtonBar::setPauseMode( bool mode )
    {
        m_pauseMode = mode;
        draw();
    }

};