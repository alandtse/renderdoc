/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "apidefs.h"
#include "rdcarray.h"
#include "rdcstr.h"
#include "stringise.h"

DOCUMENT(R"(The result of attempting to load PDB symbols for one module during callstack resolution.

.. data:: Unknown

  Status is not yet known (resolver not run).

.. data:: Loaded

  PDB was found and loaded successfully.

.. data:: ForceLoaded

  PDB was loaded by the user via force-load, bypassing GUID/age validation.

.. data:: Ignored

  Module is in the user ignore list; symbol loading was skipped.

.. data:: NotFound

  PDB could not be located in any configured symbol search path or symbol server.

.. data:: Failed

  PDB was found but failed to load (GUID/age mismatch, DIA error, etc.).

.. data:: Skipped

  Internal module (renderdoc.dll, dbghelp.dll, symsrv.dll) silently skipped.
)");
enum class PDBStatus : uint32_t
{
  Unknown,
  Loaded,
  ForceLoaded,
  Ignored,
  NotFound,
  Failed,
  Skipped,
};

DECLARE_REFLECTION_ENUM(PDBStatus);

DOCUMENT(R"(Per-module symbol load status reported after callstack resolution.)");
struct ModuleStatus
{
  DOCUMENT("");
  ModuleStatus() = default;
  ModuleStatus(const ModuleStatus &) = default;
  ModuleStatus &operator=(const ModuleStatus &) = default;

  bool operator==(const ModuleStatus &o) const
  {
    return moduleName == o.moduleName && pdbPath == o.pdbPath && status == o.status;
  }
  bool operator<(const ModuleStatus &o) const
  {
    if(!(moduleName == o.moduleName))
      return moduleName < o.moduleName;
    if(!(pdbPath == o.pdbPath))
      return pdbPath < o.pdbPath;
    if(!(status == o.status))
      return status < o.status;
    return false;
  }

  DOCUMENT("The full path of the DLL or EXE as recorded in the capture.");
  rdcstr moduleName;

  DOCUMENT("The path from which the PDB was loaded, or the last path attempted if loading failed.");
  rdcstr pdbPath;

  DOCUMENT("The load result for this module's symbols.");
  PDBStatus status = PDBStatus::Unknown;

  DOCUMENT("Human-readable explanation of the status (searched paths, error detail, etc.).");
  rdcstr statusReason;
};

DECLARE_REFLECTION_STRUCT(ModuleStatus);
