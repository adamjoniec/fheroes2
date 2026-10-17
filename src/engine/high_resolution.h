/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2025                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#pragma once

#include <string>

#include "image.h"

namespace fheroes2
{
    int graphicsScale();
    const std::string & graphicsPack();
    void configureGraphicsPack( const std::string & path, int scale );
    std::unique_ptr<Image> expandLogicalPixels( const Image & image, int scale );
    void loadHighResolutionFrame( Image & image, const std::string & type, const std::string & file, size_t frame, bool baseGameOnly = true );

    // Detach detailed pixels while the original operation writes to the logical image.
    // Raw writes outside mirrored operations discard detail, so stale pixels cannot leak.
    class HighResolutionGuard
    {
    public:
        template <class Operation>
        HighResolutionGuard( Image & output, const Image * input, Operation operation )
            : _output( output )
        {
            const int scale = graphicsScale();
            if ( scale == 1 || ( !output.highResolution() && ( !input || !input->highResolution() ) ) )
                return;
            _detail = output.takeHighResolution();
            const Image * source = input == &output ? _detail.get() : ( input ? input->highResolution() : nullptr );
            std::unique_ptr<Image> expandedInput;
            if ( input && !source ) {
                expandedInput = expandLogicalPixels( *input, scale );
                source = expandedInput.get();
            }
            if ( !_detail )
                _detail = expandLogicalPixels( output, scale );
            operation( source, *_detail, scale );
        }
        ~HighResolutionGuard()
        {
            if ( _detail && _detail->width() == _output.width() * graphicsScale() && _detail->height() == _output.height() * graphicsScale() )
                _output.setHighResolution( std::move( _detail ) );
        }
        HighResolutionGuard( const HighResolutionGuard & ) = delete;
        HighResolutionGuard & operator=( const HighResolutionGuard & ) = delete;

    private:
        Image & _output;
        std::unique_ptr<Image> _detail;
    };
}
