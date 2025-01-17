// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_RESOURCE_READER_H_
#define COMPONENTS_DATASOURCE_RESOURCE_READER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

// Helper to access Vivaldi resources
class ResourceReader {
 public:
  // Try to open the resource. If this does not succeed, `IsValid()` will return
  // false and `GetError()` will return error details.
  ResourceReader(std::string resource_path);
  ResourceReader(const ResourceReader&) = delete;
  ~ResourceReader();
  ResourceReader operator=(const ResourceReader&) = delete;

  // Check if the url points to internal Vivaldi resources. If |subpath| is not
  // null on return |*subpath| holds the resource path.
  static bool IsResourceURL(base::StringPiece url,
                            std::string* subpath = nullptr);

#if !BUILDFLAG(IS_ANDROID)
  // Get directory holding Vivaldi resource files. To simplify development in
  // non-official builds this may return source directory of vivapp/src, not the
  // directory from the build or installation. This way the changes to it can be
  // reflected without a rebuild.
  static const base::FilePath& GetResourceDirectory();
#endif

  // Convenience method to read a resource as JSON from the given resource
  // directory and resource. `resource_directory`, when not empty, should not
  // start or end with a slash. All errors are logged.
  static absl::optional<base::Value> ReadJSON(
      base::StringPiece resource_directory,
      base::StringPiece resource_name);

  static gfx::Image ReadPngImage(base::StringPiece resource_url);

  bool IsValid() const { return mapped_file_.IsValid(); }

  const uint8_t* data() const { return mapped_file_.data(); }
  size_t size() const { return mapped_file_.length(); }

  base::StringPiece as_string_view() const {
    return base::StringPiece(reinterpret_cast<const char*>(data()), size());
  }

  // Parse the asset as JSON.
  absl::optional<base::Value> ParseJSON();

  std::string GetError() const;

  // Return true if the open error was due to missing resource.
  bool IsNotFoundError() const {
    DCHECK(!IsValid());
    return not_found_error_;
  }

 private:
  base::MemoryMappedFile mapped_file_;
  std::string resource_path_;
  std::string error_;
  bool not_found_error_ = false;
};

#endif  // COMPONENTS_RESOURCES_RESOURCE_READER_H_
