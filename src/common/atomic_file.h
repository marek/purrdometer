// A file replaced whole: written to a file of its own beside it, flushed to
// disk, renamed into place and the folder flushed, so a reader sees the old file or the new and
// two writers never share a temporary file.
#pragma once

#include <string>
#include <sys/types.h>

namespace purr {

// `mode` is set before any content is written; the file and then its folder are flushed to disk. Throws
// std::runtime_error: before the rename the old file is left and nothing is left beside it; if only the
// folder could not be flushed the new file is in place but may not survive a power cut.
void replaceFile(const std::string& path, const std::string& content, mode_t mode = 0644);

} // namespace purr
