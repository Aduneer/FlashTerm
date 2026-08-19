#include "image.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ostream>
#include <vector>

#include "audio.h"
#include "text.h"

namespace FlashTerm {
namespace image {
namespace {

// Cells are about twice as tall as they are wide in every terminal font worth
// the name. Used when the terminal will not say, which is the common case.
constexpr double kDefaultCellAspect = 2.0;

// Enough for every header this reads: PNG needs 24 bytes, GIF 10, and JPEG
// wants to walk its segments but never far in practice.
constexpr std::size_t kHeaderBytes = 4096;

std::string header_of(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return {};
  std::string bytes(kHeaderBytes, '\0');
  file.read(&bytes[0], static_cast<std::streamsize>(kHeaderBytes));
  bytes.resize(static_cast<std::size_t>(file.gcount()));
  return bytes;
}

std::uint8_t byte_at(const std::string& bytes, std::size_t index) {
  return static_cast<std::uint8_t>(bytes[index]);
}

int big_endian_32(const std::string& bytes, std::size_t at) {
  return static_cast<int>((static_cast<std::uint32_t>(byte_at(bytes, at)) << 24) |
                          (static_cast<std::uint32_t>(byte_at(bytes, at + 1)) << 16) |
                          (static_cast<std::uint32_t>(byte_at(bytes, at + 2)) << 8) |
                          static_cast<std::uint32_t>(byte_at(bytes, at + 3)));
}

int big_endian_16(const std::string& bytes, std::size_t at) {
  return (byte_at(bytes, at) << 8) | byte_at(bytes, at + 1);
}

int little_endian_16(const std::string& bytes, std::size_t at) {
  return (byte_at(bytes, at + 1) << 8) | byte_at(bytes, at);
}

bool starts_with(const std::string& bytes, const char* magic, std::size_t len) {
  return bytes.size() >= len && std::memcmp(bytes.data(), magic, len) == 0;
}

// The IHDR chunk is mandatory and must come first, so the dimensions are at a
// fixed offset rather than somewhere that has to be searched for.
Size png_size(const std::string& bytes) {
  if (bytes.size() < 24) return {};
  Size size;
  size.width = big_endian_32(bytes, 16);
  size.height = big_endian_32(bytes, 20);
  return size;
}

Size gif_size(const std::string& bytes) {
  if (bytes.size() < 10) return {};
  Size size;
  size.width = little_endian_16(bytes, 6);
  size.height = little_endian_16(bytes, 8);
  return size;
}

// JPEG keeps its dimensions in whichever start-of-frame segment it happens to
// use, so the segment chain has to be walked. The frame markers are C0-CF
// except C4, C8 and CC, which are Huffman and arithmetic tables that merely
// look like frames.
Size jpeg_size(const std::string& bytes) {
  std::size_t at = 2;
  while (at + 9 < bytes.size()) {
    if (byte_at(bytes, at) != 0xFF) return {};
    const std::uint8_t marker = byte_at(bytes, at + 1);
    if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 &&
        marker != 0xCC) {
      Size size;
      size.height = big_endian_16(bytes, at + 5);
      size.width = big_endian_16(bytes, at + 7);
      return size;
    }
    const int length = big_endian_16(bytes, at + 2);
    if (length < 2) return {};
    at += 2 + static_cast<std::size_t>(length);
  }
  return {};
}

std::string environment(const char* name) {
  const char* value = std::getenv(name);
  return (value == nullptr) ? std::string() : value;
}

// Terminals that speak the kitty graphics protocol.
//
// $TERM and nothing else, which is a deliberate narrowing rather than an
// oversight. $TERM is replaced for each session -- a multiplexer, an ssh
// session or a nested terminal all set their own -- while $TERM_PROGRAM and
// $KITTY_WINDOW_ID are ordinary environment variables that are *inherited*
// and outlive the terminal that set them. Reading those is how a session
// running under something else entirely still claims to be Ghostty.
//
// Getting this wrong in the optimistic direction is the expensive mistake: the
// frame reserves room for a picture, the escape sequence is ignored by a
// terminal that never understood it, and the card is left with a hole in it.
// A card with no picture is fine; a card with a gap where one should be is
// not. So terminals whose $TERM says nothing useful -- WezTerm defaults to
// plain xterm-256color -- are expected to say so with $FLASHTERM_IMAGE.
bool terminal_speaks_kitty() {
  const std::string term = to_lowercase(environment("TERM"));
  return term.find("kitty") != std::string::npos ||
         term.find("ghostty") != std::string::npos;
}

const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The protocol carries its payload in base64, and the payload here is a file
// path -- so this never sees more than a few hundred bytes and does not need
// to be clever.
std::string base64(const std::string& input) {
  std::string out;
  out.reserve((input.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < input.size(); i += 3) {
    const std::uint32_t a = static_cast<std::uint8_t>(input[i]);
    const bool has_b = i + 1 < input.size();
    const bool has_c = i + 2 < input.size();
    const std::uint32_t b = has_b ? static_cast<std::uint8_t>(input[i + 1]) : 0;
    const std::uint32_t c = has_c ? static_cast<std::uint8_t>(input[i + 2]) : 0;
    const std::uint32_t triple = (a << 16) | (b << 8) | c;

    out += kBase64Alphabet[(triple >> 18) & 0x3F];
    out += kBase64Alphabet[(triple >> 12) & 0x3F];
    out += has_b ? kBase64Alphabet[(triple >> 6) & 0x3F] : '=';
    out += has_c ? kBase64Alphabet[triple & 0x3F] : '=';
  }
  return out;
}

// Runs `command` and returns everything it wrote to standard output.
//
// Captured rather than inherited, unlike the audio commands, because chafa's
// output is text that has to be *placed* rather than bytes that can simply be
// let through -- see draw_with_chafa. Nothing is written to the child's stdin,
// so filling the pipe cannot deadlock: the only reader is this loop.
bool capture(const audio::Command& command, std::string* output) {
  if (!audio::runnable(command)) return false;

  int fds[2];
  if (pipe(fds) != 0) return false;

  std::vector<char*> argv;
  argv.reserve(command.size() + 1);
  for (const std::string& word : command) {
    argv.push_back(const_cast<char*>(word.c_str()));
  }
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    // A converter must not read the keystrokes meant for the review, and its
    // complaints must not land in the middle of the card.
    const int null = open("/dev/null", O_RDWR);
    if (null >= 0) {
      dup2(null, STDIN_FILENO);
      dup2(null, STDERR_FILENO);
      if (null > STDERR_FILENO) close(null);
    }
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(fds[1]);
  char buffer[4096];
  ssize_t got = 0;
  while ((got = read(fds[0], buffer, sizeof(buffer))) != 0) {
    if (got < 0) {
      if (errno == EINTR) continue;
      break;
    }
    output->append(buffer, static_cast<std::size_t>(got));
  }
  close(fds[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 && !output->empty();
}

// Draws through chafa, as coloured unicode blocks.
//
// The format is forced rather than left to chafa, which is the opposite of
// what it looks like it should be. Left alone, chafa picks the kitty protocol
// astonishingly readily -- it does so even with $TERM set to xterm-256color
// and its output going down a pipe -- and this path exists precisely for the
// terminals that cannot draw one. Its guess being wrong shows up as a hole in
// the card where the picture belongs, which is the worst outcome available.
// "symbols" is the one answer that is right everywhere.
//
// Which makes the picture ordinary text, several lines of it, and text has to
// be *placed*: every line after the first would otherwise begin at column zero
// and write straight through the frame's left border. So each line is put
// where it belongs rather than simply let through.
bool draw_with_chafa(const std::string& path, const Placement& where,
                     int indent, std::ostream& out) {
  const std::string size =
      std::to_string(where.columns) + "x" + std::to_string(where.rows);
  std::string rendered;
  if (!capture({"chafa", "--format=symbols", "--animate=off", "--size=" + size,
                path},
               &rendered)) {
    return false;
  }

  // chafa brackets its output with "hide cursor" and "show cursor". Both have
  // to go: the trailing one sits on a line of its own and would otherwise be
  // counted as one more row than the frame reserved, overrunning the border --
  // and hiding the cursor on every redraw of a review would flicker it.
  for (const char* control : {"\033[?25l", "\033[?25h"}) {
    for (std::size_t at = rendered.find(control); at != std::string::npos;
         at = rendered.find(control, at)) {
      rendered.erase(at, std::strlen(control));
    }
  }

  const std::vector<std::string> lines = split(rendered, '\n');
  int drawn = 0;
  for (const std::string& line : lines) {
    // chafa ends with a newline, so the last piece is empty and is not a row.
    if (line.empty()) continue;
    if (drawn > 0) {
      // Down one and back to the picture's own left edge. Never a bare "\n",
      // which would scroll the screen if the frame happened to reach the
      // bottom of it and take the whole card up with it.
      out << "\r\033[1B";
      if (indent > 0) out << "\033[" << indent << "C";
    }
    out << line;
    ++drawn;
  }
  // Back to the row this started on, so that both ways of drawing leave the
  // cursor in the same place and the caller needs to know which was used.
  if (drawn > 1) out << "\r\033[" << (drawn - 1) << "A";
  out.flush();
  return drawn > 0;
}
}  // namespace

Size read_size(const std::string& path) {
  if (path.empty()) return {};
  const std::string bytes = header_of(path);
  if (bytes.size() < 10) return {};

  Size size;
  if (starts_with(bytes, "\x89PNG\r\n\x1a\n", 8)) {
    size = png_size(bytes);
  } else if (starts_with(bytes, "GIF87a", 6) || starts_with(bytes, "GIF89a", 6)) {
    size = gif_size(bytes);
  } else if (starts_with(bytes, "\xFF\xD8", 2)) {
    size = jpeg_size(bytes);
  }
  // A header that parses to nonsense is treated as no picture rather than
  // trusted: every later calculation divides by these.
  if (!size.valid()) return {};
  return size;
}

Protocol detect() {
  const std::string asked = to_lowercase(trim(environment("FLASHTERM_IMAGE")));
  if (asked == "none" || asked == "off") return Protocol::kNone;
  if (asked == "kitty") return Protocol::kKitty;
  if (asked == "chafa") return Protocol::kChafa;

  if (terminal_speaks_kitty()) return Protocol::kKitty;
  if (audio::runnable({"chafa"})) return Protocol::kChafa;
  return Protocol::kNone;
}

bool available() { return detect() != Protocol::kNone; }

std::string protocol_name() {
  switch (detect()) {
    case Protocol::kKitty: return "kitty";
    case Protocol::kChafa: return "chafa";
    case Protocol::kNone: break;
  }
  return "none";
}

double cell_aspect() {
  winsize window{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 &&
      window.ws_row > 0 && window.ws_xpixel > 0 && window.ws_ypixel > 0) {
    const double cell_width =
        static_cast<double>(window.ws_xpixel) / window.ws_col;
    const double cell_height =
        static_cast<double>(window.ws_ypixel) / window.ws_row;
    if (cell_width > 0.0) return cell_height / cell_width;
  }
  return kDefaultCellAspect;
}

Placement fit(const Size& size, int max_columns, int max_rows,
              double aspect) {
  if (!size.valid() || max_columns <= 0 || max_rows <= 0 || aspect <= 0.0) {
    return {};
  }

  // Columns the picture would need if it were given every row on offer. The
  // cell aspect is what turns a ratio of pixels into a ratio of cells.
  const double ratio = static_cast<double>(size.width) / size.height;
  const double wanted = max_rows * ratio * aspect;

  Placement where;
  if (wanted <= max_columns) {
    where.columns = static_cast<int>(std::lround(wanted));
    where.rows = max_rows;
  } else {
    // Too wide for the frame, so width becomes the constraint and the picture
    // gets fewer rows than it was offered.
    where.columns = max_columns;
    where.rows = static_cast<int>(std::lround(max_columns / (ratio * aspect)));
  }
  // Rounding can take either side to zero for an extreme ratio, and a box with
  // no area draws nothing at all.
  if (where.columns < 1) where.columns = 1;
  if (where.rows < 1) where.rows = 1;
  return where;
}

bool draw(const std::string& path, const Placement& where, int indent,
          std::ostream& out) {
  if (path.empty() || where.empty()) return false;

  switch (detect()) {
    case Protocol::kNone:
      return false;
    case Protocol::kChafa:
      return draw_with_chafa(path, where, indent, out);
    case Protocol::kKitty:
      break;
  }

  // a=T transmit and display, f=100 the file is a PNG-or-whatever the terminal
  // can decode, t=f the payload is a path rather than the picture itself,
  // C=1 leave the cursor alone.
  out << "\033_Ga=T,f=100,t=f,C=1,c=" << where.columns << ",r=" << where.rows
      << ";" << base64(path) << "\033\\";
  out.flush();
  return true;
}
}  // namespace image
}  // namespace FlashTerm
