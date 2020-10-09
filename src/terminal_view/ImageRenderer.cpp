/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <terminal_view/ImageRenderer.h>

namespace terminal::view {

ImageRenderer::ImageRenderer(crispy::atlas::CommandListener& _commandListener,
                             crispy::atlas::TextureAtlasAllocator& _colorAtlasAllocator,
                             Size const& _cellSize) :
    imagePool_{},
    commandListener_{ _commandListener },
    cellSize_{ _cellSize },
    atlas_{ _colorAtlasAllocator }
{
    (void) _colorAtlasAllocator; // TODO
}

void ImageRenderer::setCellSize(Size const& _cellSize)
{
    cellSize_ = _cellSize;
    // TODO: recompute slices here?
}


void ImageRenderer::renderImage(Image const& _image,
                                Coordinate _offset,
                                Size _extent)
{
    std::cout << fmt::format(
        "ImageRenderer.renderImage: {} at {} / {}\n",
        _image, _offset, _extent
    );
    (void) _image;
    (void) _offset;
    (void) _extent;
}

void ImageRenderer::clearCache()
{
    atlas_.clear();
}

} // end namespace
