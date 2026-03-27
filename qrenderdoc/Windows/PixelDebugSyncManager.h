/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include "Code/Interface/QRDInterface.h"

class ShaderViewer;

// Holds one value per viewer for a single variable path. When a viewer does not have
// that variable in its current state (e.g. it has already exited), present is false.
struct VarDiff
{
  QString path;
  QList<ShaderVariable> values;    // one entry per viewer in group order
  QList<bool> present;             // false when the corresponding viewer lacks this variable
  bool divergent = false;          // true when any pair exceeds the comparison threshold
  // Per-component divergence for leaf variables (rows*columns entries, empty for struct parents).
  // componentDivergent[i] is true when component i differs across any viewer pair.
  QList<bool> componentDivergent;
};

// A set of ShaderViewer instances whose execution is kept in lock-step.
// Each group is scoped to a specific shader+event combination so that viewers debugging
// the same shader at the same draw call are automatically grouped together.
struct SyncGroup
{
  uint32_t id = 0;
  QString name;
  // Scope: the shader and event that define this group's identity. Viewers are auto-grouped
  // by (shaderScopeId, eventScopeId) so different draw calls or shaders get separate groups.
  ResourceId shaderScopeId;
  uint32_t eventScopeId = 0;
  float threshold = 0.001f;    // max absolute difference before a float is flagged divergent
  bool ignoreIntDivergence = false;     // if true, integer/bool differences are not flagged
  bool autoBreakOnDivergence = true;    // stop "run forward" when viewers hit different instructions
  bool autoBreakOnVarDivergence = false;    // stop "run forward" when any variable newly diverges
  QList<ShaderViewer *> viewers;
  // Serial numbers: each viewer that has ever joined this group gets a monotonically-increasing
  // number (never reused). Labels take the form "<groupId>-<serial>", e.g. "1-1", "1-2".
  uint32_t nextSerialNumber = 1;
  QMap<ShaderViewer *, uint32_t> viewerSerials;
};

// Manages one or more SyncGroups. Lives as a singleton per application (owned by QApplication).
// ShaderViewer registers/unregisters itself; PixelDebugSyncPanel observes via signals.
class PixelDebugSyncManager : public QObject
{
  Q_OBJECT

public:
  static PixelDebugSyncManager *instance();

  // Viewer membership
  void addViewerToGroup(uint32_t groupId, ShaderViewer *viewer);
  void removeViewer(ShaderViewer *viewer);

  // Find an existing group scoped to the same shader+event as the given viewer, or create one.
  // This is the preferred join path — callers do not need to manage group IDs manually.
  uint32_t findOrCreateGroupForViewer(const ShaderViewer *viewer);

  uint32_t groupForViewer(const ShaderViewer *viewer) const;
  // Returns the display label for a viewer in the form "<groupId>-<serial>", e.g. "1-1".
  // Returns an empty string if the viewer is not in the group.
  QString serialLabelForViewer(uint32_t groupId, const ShaderViewer *viewer) const;
  const SyncGroup *getGroup(uint32_t groupId) const;
  SyncGroup *getGroup(uint32_t groupId);
  QList<uint32_t> allGroupIds() const;

  void setThreshold(uint32_t groupId, float threshold);
  void setIgnoreIntDivergence(uint32_t groupId, bool ignore);
  void setAutoBreakOnDivergence(uint32_t groupId, bool enabled);
  void setAutoBreakOnVarDivergence(uint32_t groupId, bool enabled);

  // Called when a viewer changes progress (step/run) so group members are driven to the same
  // step index.
  void onViewerStepped(ShaderViewer *source);

  // Called when a viewer changes view mode (source/disassembly/int/float), propagating that mode
  // to all other viewers in the group.
  void onViewerDisplayChanged(ShaderViewer *source);

  // Comparison helpers used by PixelDebugSyncPanel
  QList<VarDiff> computeDiffs(uint32_t groupId) const;
  bool hasBranchDivergence(uint32_t groupId) const;

  // Emit stepCompleted (and branchDivergenceDetected if applicable) for the given group.
  // Used by ShaderViewer after bulk-stepping operations that bypass the normal notify path.
  void notifyStepCompleted(uint32_t groupId);

  // Format a ShaderVariable value as a display string (numeric types; no resource lookup needed).
  static QString formatVarValue(const ShaderVariable &var);

  // Returns true when two shader variables differ beyond the given absolute threshold.
  // Public so ShaderViewer can use it for inline divergence checks.
  static bool varsAreDivergent(const ShaderVariable &a, const ShaderVariable &b, float threshold);

signals:
  void groupListChanged();
  void groupUpdated(uint32_t groupId);
  void stepCompleted(uint32_t groupId);
  void branchDivergenceDetected(uint32_t groupId, uint32_t instrA, uint32_t instrB);

private:
  explicit PixelDebugSyncManager(QObject *parent = NULL);

  int findGroupIndex(uint32_t groupId) const;

  static void collectLeafPaths(const ShaderVariable &var, const QString &prefix,
                               QList<QString> &paths);
  static ShaderVariable findVarByPath(const QList<ShaderVariable> &vars, const QString &path);

  QList<SyncGroup> m_Groups;
  uint32_t m_NextGroupId = 1;
};
