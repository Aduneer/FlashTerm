#pragma once
#include <iosfwd>
#include <string>

namespace FlashTerm {
namespace image {

// Drawing a picture inside the card frame.
//
// Shaped by three things the terminal will not do for you, each established by
// experiment rather than by reading a specification:
//
//   * A terminal that speaks the kitty graphics protocol scales an image into
//     a box of cells you name. Give it only `r` and it preserves the aspect
//     ratio; give it both `r` and `c` and it *stretches* to fill them. So the
//     aspect arithmetic has to happen here, and `fit()` is where.
//   * The image can be transmitted as a file path, so nothing decodes or
//     re-encodes it: the escape sequence is about sixty bytes however large
//     the picture is. That matters on a screen the review loop redraws after
//     every keypress.
//   * After drawing, the cursor is left inside the image rather than below it,
//     so a frame is drawn first and the picture dropped into it afterwards.
//     `draw()` therefore never moves the cursor and leaves that to the caller.

// Pixel dimensions of an image file.
struct Size {
  int width = 0;
  int height = 0;

  bool valid() const { return width > 0 && height > 0; }
};

// Reads only the header, so the cost does not scale with the picture: PNG, GIF
// and JPEG keep their dimensions within the first few bytes or the first few
// segments. An unreadable file, or one in some other format, comes back
// invalid -- which the caller shows as a card without a picture rather than as
// an error, the same way a missing recording is not worth interrupting a
// review over.
Size read_size(const std::string& path);

// How a terminal is willing to be sent a picture.
enum class Protocol {
  kNone,   // Draw nothing. Every deck still loads and reviews.
  kKitty,  // Built in: an escape sequence naming the file. No tools required.
  kChafa,  // Shell out to chafa for coloured unicode blocks, which need no
           // graphics support of any kind and so work everywhere.
};

// $FLASHTERM_IMAGE wins when it names a protocol ("none", "kitty", "chafa"),
// because what a terminal can display is exactly the kind of thing automatic
// detection gets wrong on somebody else's setup -- and because a golden test
// must be able to pin it. Otherwise: the kitty protocol when $TERM or
// $TERM_PROGRAM says the terminal speaks it, then chafa if it is installed,
// then nothing.
Protocol detect();

// True when a picture would be drawn at all. Review asks before reserving room
// for one, the way audio::available() is asked before offering the "a" key.
bool available();

// The protocol in use, for --help and for saying why a card shows no picture.
std::string protocol_name();

// A box of terminal cells.
struct Placement {
  int columns = 0;
  int rows = 0;

  bool empty() const { return columns <= 0 || rows <= 0; }
};

// The largest box no bigger than `max_columns` by `max_rows` that keeps the
// picture's proportions. Cells are taller than they are wide, so this needs
// `cell_aspect` -- height divided by width -- or a 4:1 photograph comes out
// looking 8:1.
Placement fit(const Size& size, int max_columns, int max_rows,
              double cell_aspect);

// A cell's height divided by its width. Asked of the terminal, which usually
// declines to answer: ws_xpixel and ws_ypixel are zero more often than not, so
// the fallback is the ratio almost every terminal font actually has.
double cell_aspect();

// Draws `path` at the cursor, occupying at most `where`.
//
// `indent` is the column the picture starts at, counted from the left edge of
// the screen. Needed because one of the two ways of drawing produces lines of
// text rather than a single escape, and every line after the first has to be
// put back at that column instead of beginning at zero.
//
// Leaves the cursor on the row it started on, whichever way the picture was
// drawn -- the escape-sequence way never moves it, and the text way puts it
// back -- so the caller's arithmetic does not depend on which one ran.
//
// Deliberately does not clear what is underneath: the caller has already drawn
// a frame around this space and is responsible for stepping back out of it.
// Returns false when nothing was drawn, which is not an error worth
// interrupting a review over.
bool draw(const std::string& path, const Placement& where, int indent,
          std::ostream& out);
}  // namespace image
}  // namespace FlashTerm
