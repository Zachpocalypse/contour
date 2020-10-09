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
#pragma once

#include <terminal/Commands.h>
#include <terminal/Size.h>

#include <fmt/format.h>

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <vector>

namespace terminal {

// XXX DRAFT
// Do we want to keep an Image that keeps the whole image together, and then cut it into grid-cell
// slices (requires reference counting on main image)?
// Or do we want to deal with Image slices right away and just keep those?
// The latter doesn't require reference counting.

/// Image resize hints are used to properly fit/fill the area to place the image onto.
enum class ImageResize {
    NoResize,
    ResizeToFit, // default
    ResizeToFill,
    StretchToFill,
};

/// Image alignment policy are used to properly align the image to a given spot when not fully
/// filling the area this image as to be placed to.
enum class ImageAlignment {
    TopStart,
    TopCenter,
    TopEnd,
    MiddleStart,
    MiddleCenter, // default
    MiddleEnd,
    BottomStart,
    BottomCenter,
    BottomEnd
};

/**
 * Represents an image that can be displayed in the terminal by being placed into the grid cells
 */
class Image {
  public:
    using Data = std::vector<uint8_t>; // raw RGBA data
    using Deleter = std::function<void(Image const&)>;

    /// Constructs an RGBA image.
    ///
    /// @param _data      RGBA buffer data
    /// @param _pixelSize image dimensionss in pixels
    /// @param _deleter   custom deleter-callback to be invoked upon image destruction
    Image(Data _data, Size _pixelSize, Deleter _deleter = Deleter{}) :
        data_{ move(_data) },
        size_{ _pixelSize },
        deleter_{ move(_deleter) },
        refCount_{ 0 }
    {}

    Data const& data() const noexcept { return data_; }
    constexpr Size size() const noexcept { return size_; }
    constexpr int width() const noexcept { return size_.width; }
    constexpr int height() const noexcept { return size_.height; }

    constexpr void ref() const noexcept
    {
        ++refCount_;
    }

    void unref() const
    {
        --refCount_;
        if (refCount_ == 0)
        {
            if constexpr (!std::is_same_v<Deleter, void>)
                deleter_(*this);
        }
    }

    constexpr int refCount() const noexcept { return refCount_; }

  private:
    Data data_;
    Size size_;
    Deleter deleter_;
    mutable int refCount_;
};

/// Holds a save reference to an Image by using reference-counting.
class ImageRef {
  public:
    explicit constexpr ImageRef(Image const& _image) noexcept :
        image_{&_image}
    {
        image_->ref();
    }

    ~ImageRef()
    {
        if (image_)
            image_->unref();
    }

    ImageRef(ImageRef const& v)
    {
        image_ = v.image_;
        image_->ref();
    }

    ImageRef(ImageRef&& v) noexcept
    {
        image_ = v.image_;
        v.image_ = nullptr;
    }

    ImageRef& operator=(ImageRef const& v)
    {
        if (&v != this)
        {
            image_->unref();
            image_ = v.image_;
            image_->ref();
        }
        return *this;
    }

    ImageRef& operator=(ImageRef&& v) noexcept
    {
        if (&v != this)
        {
            image_ = v.image_;
            v.image_ = nullptr;
        }
        return *this;
    }

    constexpr Image const& get() const noexcept { return *image_; }

  private:
    Image const* image_;
};

constexpr bool operator==(ImageRef const& _a, ImageRef const& _b) noexcept
{
    return &_a.get() == &_b.get();
}

constexpr bool operator!=(ImageRef const& _a, ImageRef const& _b) noexcept
{
    return !(_a == _b);
}

/// An ImageFragment holds a graphical image that ocupies one full grid cell.
class ImageFragment {
  public:
    /// @param _image  the Image this fragment is being cut off from
    /// @param _offset 0-based offset in pixels into the image
    /// @param _size   fragment size
    ImageFragment(Image const& _image, Coordinate _offset, Size _size) :
        image_{ _image },
        offset_{ _offset },
        size_{ _size }
    {
    }

    ImageFragment(ImageFragment const&) = default;
    ImageFragment(ImageFragment&&) noexcept = default;
    ImageFragment& operator=(ImageFragment const&) = default;
    ImageFragment& operator=(ImageFragment&&) noexcept = default;

    Image const& image() const noexcept { return image_.get(); }

    Coordinate offset() const noexcept { return offset_; }
    Size size() const noexcept { return size_; }

    /// Extracts the data from the image that is to be rendered. TODO: use spans instead.
    Image::Data data() const
    {
        auto const& fullImageData = image_.get().data();

        Image::Data fragData;
        fragData.reserve(size_.width * size_.height * 4); // RBGA
        for (int y = 0; y < size_.height; ++y)
        {
            std::copy(
                &fullImageData[y * size_.width * 4],
                &fullImageData[y * size_.width * 4] + size_.width * 4,
                std::back_inserter(fragData)
            );
        }

        return fragData;
    }

  private:
    ImageRef image_;
    Coordinate offset_;
    Size size_;
};

/**
 * ImageRaster contains properties of an image to be rastered from pixels into grid cells.
 */
struct RasterizedImage
{
    ImageRef image;

    /// Number of grid cells to span the pixel image onto.
    Size cellSpan;

    /// Alignment policy of the image inside the raster size.
    ImageAlignment alignmentPolicy = ImageAlignment::TopStart;

    /// Image resize policy
    ImageResize resizePolicy = ImageResize::NoResize;

    /// @returns number of pixels in X and Y dimension one grid cell has to fill.
    Size cellSize() const noexcept
    {
        return image.get().size() / cellSpan;
    }

    /// @returns an ImageFragment for a grid cell at given coordinate @p _pos of the rasterized image.
    ImageFragment fragment(Coordinate _pos) const
    {
        auto const cellUnitWidth = image.get().width() / cellSpan.width;

        auto const pixelOffset = Coordinate{
            _pos.row * image.get().width(),
            _pos.column * cellUnitWidth
        };

        return ImageFragment{image.get(), pixelOffset, cellSpan};
    }
};

/// Named image, as used for decoupling image upload and image render.
class NamedImage {
  public:
    NamedImage(std::string _name, uint64_t _createdAt, Image const& _image);

    std::string const& name() const noexcept { return name_; }
    uint64_t createdAt() const noexcept { return createdAt_; }

    Image const& image() const noexcept { return image_.get(); }

  private:
    std::string name_;
    uint64_t createdAt_;
    ImageRef image_;
};

inline bool operator<(NamedImage const& _a, NamedImage const& _b) noexcept
{
    if (_a.createdAt() < _b.createdAt())
        return true;

    if (_a.createdAt() == _b.createdAt())
        return _a.name() < _b.name();

    return false;
}

/// Highlevel Image Storage Pool.
///
/// Stores RGBA images in host memory, also taking care of eviction.
class ImagePool {
  public:
    /// Creates an RGBA image of given size in pixels.
    ImageRef create(Image::Data _data, Size _size);

    /// Creates an RGB image of given size in pixels.
    ImageRef create(std::vector<RGBColor> const& _data, Size _size);

    // XXX future API, needed for the "Good Image Protocol"
    //void removeNamedImage(std::string const& _name);

    int imageCount() const noexcept;

    /// Removes given image from image pool.
    void remove(Image const& _image);

  private:
    std::list<Image> images_;
    std::list<RasterizedImage> instances_;
    //XXX std::list<NamedImage> namedImages_;
};

} // end namespace

namespace fmt
{
    template <>
    struct formatter<terminal::Image>
    {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const terminal::Image& _image, FormatContext& ctx)
        {
            return format_to(
                ctx.out(),
                "Image<{}, size={}, refCount={}>",
                (void const*) &_image,
                _image.size(),
                _image.refCount()
            );
        }
    };

    template <>
    struct formatter<terminal::ImageFragment>
    {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx)
        {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const terminal::ImageFragment& _fragment, FormatContext& ctx)
        {
            return format_to(
                ctx.out(),
                "ImageFragment<{}, offset={}, size={}>",
                _fragment.image(),
                _fragment.offset(),
                _fragment.size()
            );
        }
    };
}
