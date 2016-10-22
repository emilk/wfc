#ifndef WFC_RGBA_HPP
#define WFC_RGBA_HPP

struct RGBA
{
	uint8_t r, g, b, a;

    bool operator==(RGBA o) const
    {
        return r == o.r
            && g == o.g
            && b == o.b
            && a == o.a;
    }
};
static_assert(sizeof(RGBA) == 4, "");

#endif

