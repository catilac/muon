// Copyright (c) 2017 The Brave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BRAVE_UTILITY_IMPORTER_BRAVE_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
#define BRAVE_UTILITY_IMPORTER_BRAVE_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_

#include <vector>

#include "chrome/utility/importer/external_process_importer_bridge.h"

struct ImportedCookieEntry;

class BraveExternalProcessImporterBridge :
                                      public ExternalProcessImporterBridge {
 public:
  // |observer| must outlive this object.
  BraveExternalProcessImporterBridge(
      base::Value localized_strings,
      scoped_refptr<chrome::mojom::ThreadSafeProfileImportObserverPtr>
          observer);

  void SetCookies(const std::vector<ImportedCookieEntry>& cookies);
 private:
  ~BraveExternalProcessImporterBridge() override;

  DISALLOW_COPY_AND_ASSIGN(BraveExternalProcessImporterBridge);
};

#endif  // BRAVE_UTILITY_IMPORTER_BRAVE_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
