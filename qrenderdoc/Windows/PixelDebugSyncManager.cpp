/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PixelDebugSyncManager.h"
#include <QApplication>
#include <QSet>
#include <cmath>
#include "ShaderViewer.h"

static PixelDebugSyncManager *s_Instance = NULL;

PixelDebugSyncManager *PixelDebugSyncManager::instance()
{
  if(!s_Instance)
    s_Instance = new PixelDebugSyncManager(qApp);
  return s_Instance;
}

PixelDebugSyncManager::PixelDebugSyncManager(QObject *parent) : QObject(parent)
{
  // Clear the singleton pointer when this object is destroyed (e.g. when qApp exits) so that
  // any late callers of instance() get NULL rather than a dangling pointer.
  QObject::connect(this, &QObject::destroyed, [](QObject *) { s_Instance = NULL; });
}

uint32_t PixelDebugSyncManager::findOrCreateGroupForViewer(const ShaderViewer *viewer)
{
  ResourceId shaderId = viewer->shaderResourceId();
  uint32_t eventId = viewer->debugEventId();

  // Search for an existing group scoped to the same shader+event.
  for(const SyncGroup &g : m_Groups)
  {
    if(g.shaderScopeId == shaderId && g.eventScopeId == eventId)
      return g.id;
  }

  // None found — create a new scoped group.
  SyncGroup g;
  g.id = m_NextGroupId++;
  g.name = QFormatStr("Group %1").arg(m_Groups.size() + 1);
  g.shaderScopeId = shaderId;
  g.eventScopeId = eventId;
  m_Groups.push_back(g);
  emit groupListChanged();
  return g.id;
}

void PixelDebugSyncManager::addViewerToGroup(uint32_t groupId, ShaderViewer *viewer)
{
  int idx = findGroupIndex(groupId);
  if(idx < 0)
    return;

  SyncGroup &g = m_Groups[idx];
  g.viewers.push_back(viewer);
  g.viewerSerials[viewer] = g.nextSerialNumber++;

  // Force the newly-added viewer to the group's canonical state.
  if(g.viewers.size() > 1)
  {
    ShaderViewer *reference = g.viewers[0];
    viewer->syncStep(reference->CurrentStep());
    viewer->syncDebugMode(reference->isSourceDebugging(), reference->isIntView(),
                          reference->isFloatView());
  }

  emit groupUpdated(groupId);
}

void PixelDebugSyncManager::onViewerDisplayChanged(ShaderViewer *source)
{
  uint32_t groupId = groupForViewer(source);
  if(groupId == ~0U)
    return;

  const SyncGroup *g = getGroup(groupId);
  if(!g || g->viewers.size() < 2)
    return;

  bool sourceMode = source->isSourceDebugging();
  bool intMode = source->isIntView();
  bool floatMode = source->isFloatView();

  for(ShaderViewer *viewer : g->viewers)
  {
    if(viewer == source)
      continue;
    viewer->syncDebugMode(sourceMode, intMode, floatMode);
  }
}

void PixelDebugSyncManager::removeViewer(ShaderViewer *viewer)
{
  for(int gi = 0; gi < m_Groups.size(); gi++)
  {
    SyncGroup &g = m_Groups[gi];
    int vi = g.viewers.indexOf(viewer);
    if(vi >= 0)
    {
      g.viewers.removeAt(vi);
      if(g.viewers.isEmpty())
      {
        // Auto-destroy the group when the last viewer leaves so the next debug session
        // starts with a fresh group rather than inheriting a stale empty one.
        m_Groups.removeAt(gi);
        emit groupListChanged();
      }
      else
      {
        emit groupUpdated(g.id);
      }
      return;
    }
  }
}

uint32_t PixelDebugSyncManager::groupForViewer(const ShaderViewer *viewer) const
{
  for(const SyncGroup &g : m_Groups)
  {
    if(g.viewers.contains(const_cast<ShaderViewer *>(viewer)))
      return g.id;
  }
  return ~0U;
}

QString PixelDebugSyncManager::serialLabelForViewer(uint32_t groupId, const ShaderViewer *viewer) const
{
  const SyncGroup *g = getGroup(groupId);
  if(!g)
    return {};
  auto it = g->viewerSerials.find(const_cast<ShaderViewer *>(viewer));
  if(it == g->viewerSerials.end())
    return {};
  return QFormatStr("%1-%2").arg(groupId).arg(it.value());
}

const SyncGroup *PixelDebugSyncManager::getGroup(uint32_t groupId) const
{
  int idx = findGroupIndex(groupId);
  return idx >= 0 ? &m_Groups[idx] : NULL;
}

SyncGroup *PixelDebugSyncManager::getGroup(uint32_t groupId)
{
  int idx = findGroupIndex(groupId);
  return idx >= 0 ? &m_Groups[idx] : NULL;
}

QList<uint32_t> PixelDebugSyncManager::allGroupIds() const
{
  QList<uint32_t> ids;
  for(const SyncGroup &g : m_Groups)
    ids.push_back(g.id);
  return ids;
}

void PixelDebugSyncManager::setThreshold(uint32_t groupId, float threshold)
{
  SyncGroup *g = getGroup(groupId);
  if(g)
  {
    g->threshold = threshold;
    emit groupUpdated(groupId);
  }
}

void PixelDebugSyncManager::setIgnoreIntDivergence(uint32_t groupId, bool ignore)
{
  SyncGroup *g = getGroup(groupId);
  if(g)
  {
    g->ignoreIntDivergence = ignore;
    emit groupUpdated(groupId);
  }
}

void PixelDebugSyncManager::setAutoBreakOnDivergence(uint32_t groupId, bool enabled)
{
  SyncGroup *g = getGroup(groupId);
  if(g)
  {
    g->autoBreakOnDivergence = enabled;
    emit groupUpdated(groupId);
  }
}

void PixelDebugSyncManager::setAutoBreakOnVarDivergence(uint32_t groupId, bool enabled)
{
  SyncGroup *g = getGroup(groupId);
  if(g)
  {
    g->autoBreakOnVarDivergence = enabled;
    emit groupUpdated(groupId);
  }
}

void PixelDebugSyncManager::onViewerStepped(ShaderViewer *source)
{
  uint32_t groupId = groupForViewer(source);
  if(groupId == ~0U)
    return;

  const SyncGroup *g = getGroup(groupId);
  if(!g || g->viewers.size() < 2)
    return;

  uint32_t targetStep = source->CurrentStep();

  // Drive all peer viewers to the same step. syncStep() is a re-entry-safe wrapper that
  // prevents those viewers from bouncing the notification back here.
  for(ShaderViewer *viewer : g->viewers)
  {
    if(viewer == source)
      continue;
    viewer->syncStep(targetStep);
  }

  notifyStepCompleted(groupId);
}

QList<VarDiff> PixelDebugSyncManager::computeDiffs(uint32_t groupId) const
{
  const SyncGroup *g = getGroup(groupId);
  if(!g || g->viewers.size() < 2)
    return {};

  // Collect all unique leaf variable paths across all viewers
  QSet<QString> allPathsSet;
  for(ShaderViewer *viewer : g->viewers)
  {
    QList<QString> paths;
    for(const ShaderVariable &var : viewer->GetCurrentVariables())
      collectLeafPaths(var, QString(var.name), paths);
    for(const QString &p : paths)
      allPathsSet.insert(p);
  }

  QList<QString> allPaths = allPathsSet.values();
  std::sort(allPaths.begin(), allPaths.end());

  QList<VarDiff> diffs;
  diffs.reserve(allPaths.size());

  for(const QString &path : allPaths)
  {
    VarDiff diff;
    diff.path = path;

    for(ShaderViewer *viewer : g->viewers)
    {
      ShaderVariable v = findVarByPath(viewer->GetCurrentVariables(), path);
      // An empty name signals "not found"
      diff.present.push_back(!v.name.empty());
      diff.values.push_back(v);
    }

    // Find the first present viewer's type for the ignoreIntDivergence gate.
    // Unknown type (e.g. DXBC temp registers) is treated as float.
    VarType firstType = VarType::Float;
    for(int i = 0; i < diff.present.size(); i++)
    {
      if(diff.present[i])
      {
        firstType = (diff.values[i].type == VarType::Unknown) ? VarType::Float : diff.values[i].type;
        break;
      }
    }
    bool isIntOrBool =
        firstType != VarType::Float && firstType != VarType::Half && firstType != VarType::Double;

    // Flag divergent if any present pair exceeds the threshold.
    // Comparing all pairs (not just vs viewer 0) catches divergence when viewer 0 is absent.
    if(!g->ignoreIntDivergence || !isIntOrBool)
    {
      for(int i = 0; i < diff.values.size() && !diff.divergent; i++)
      {
        if(!diff.present[i])
          continue;
        for(int j = i + 1; j < diff.values.size(); j++)
        {
          if(!diff.present[j])
            continue;
          if(varsAreDivergent(diff.values[i], diff.values[j], g->threshold))
          {
            diff.divergent = true;
            break;
          }
        }
      }
    }

    // Compute per-component divergence for leaf variables (no sub-members).
    // Use the first present viewer for shape/count; compare all present pairs.
    int firstPresent = -1;
    for(int i = 0; i < diff.present.size(); i++)
    {
      if(diff.present[i])
      {
        firstPresent = i;
        break;
      }
    }
    if(diff.divergent && firstPresent >= 0 && diff.values[firstPresent].members.empty())
    {
      const ShaderVariable &ref = diff.values[firstPresent];
      uint32_t count = (uint32_t)ref.rows * (uint32_t)ref.columns;
      diff.componentDivergent.clear();
      for(uint32_t ci = 0; ci < count; ci++)
        diff.componentDivergent.push_back(false);

      bool allFlagged = false;
      for(int i = firstPresent; i < diff.values.size() && !allFlagged; i++)
      {
        if(!diff.present[i])
          continue;
        for(int j = i + 1; j < diff.values.size() && !allFlagged; j++)
        {
          if(!diff.present[j])
            continue;
          const ShaderVariable &a = diff.values[i];
          const ShaderVariable &b = diff.values[j];
          VarType typeA = (a.type == VarType::Unknown) ? VarType::Float : a.type;
          VarType typeB = (b.type == VarType::Unknown) ? VarType::Float : b.type;
          if(a.rows != b.rows || a.columns != b.columns || typeA != typeB)
          {
            for(bool &c : diff.componentDivergent)
              c = true;
            allFlagged = true;
            break;
          }
          switch(typeA)
          {
            case VarType::Double:
              for(uint32_t ci = 0; ci < count; ci++)
                if(fabs(a.value.f64v[ci] - b.value.f64v[ci]) > (double)g->threshold)
                  diff.componentDivergent[(int)ci] = true;
              break;
            case VarType::Half:
              for(uint32_t ci = 0; ci < count; ci++)
                if(fabsf((float)a.value.f16v[ci] - (float)b.value.f16v[ci]) > g->threshold)
                  diff.componentDivergent[(int)ci] = true;
              break;
            case VarType::Float:
              for(uint32_t ci = 0; ci < count; ci++)
                if(fabsf(a.value.f32v[ci] - b.value.f32v[ci]) > g->threshold)
                  diff.componentDivergent[(int)ci] = true;
              break;
            default:
              for(uint32_t ci = 0; ci < count; ci++)
                if(a.value.u32v[ci] != b.value.u32v[ci])
                  diff.componentDivergent[(int)ci] = true;
              break;
          }
        }
      }
    }

    diffs.push_back(diff);
  }

  // Sort divergent entries first, then alphabetically within each group
  std::stable_sort(diffs.begin(), diffs.end(), [](const VarDiff &a, const VarDiff &b) {
    if(a.divergent != b.divergent)
      return a.divergent > b.divergent;
    return a.path < b.path;
  });

  return diffs;
}

void PixelDebugSyncManager::notifyStepCompleted(uint32_t groupId)
{
  emit stepCompleted(groupId);

  if(hasBranchDivergence(groupId))
  {
    const SyncGroup *g = getGroup(groupId);
    if(g && g->viewers.size() >= 2)
    {
      uint32_t refInstr = g->viewers[0]->GetCurrentInstruction();
      for(int i = 1; i < g->viewers.size(); i++)
      {
        uint32_t instr = g->viewers[i]->GetCurrentInstruction();
        if(instr != refInstr)
        {
          emit branchDivergenceDetected(groupId, refInstr, instr);
          break;
        }
      }
    }
  }
}

bool PixelDebugSyncManager::hasBranchDivergence(uint32_t groupId) const
{
  const SyncGroup *g = getGroup(groupId);
  if(!g || g->viewers.size() < 2)
    return false;

  uint32_t refInstr = g->viewers[0]->GetCurrentInstruction();
  for(int i = 1; i < g->viewers.size(); i++)
  {
    if(g->viewers[i]->GetCurrentInstruction() != refInstr)
      return true;
  }
  return false;
}

QString PixelDebugSyncManager::formatVarValue(const ShaderVariable &var)
{
  if(!var.members.empty())
    return lit("{...}");

  if(var.rows == 0 || var.columns == 0)
    return lit("—");

  uint32_t count = (uint32_t)var.rows * (uint32_t)var.columns;
  VarType type = var.type;

  QStringList parts;
  parts.reserve((int)count);

  // Unknown type (e.g. DXBC temp registers) defaults to float display
  if(type == VarType::Unknown)
    type = VarType::Float;

  for(uint32_t i = 0; i < count; i++)
  {
    switch(type)
    {
      case VarType::Double: parts.push_back(QString::number(var.value.f64v[i], 'g', 6)); break;
      case VarType::Half:
        parts.push_back(QString::number((double)(float)var.value.f16v[i], 'g', 6));
        break;
      case VarType::Float:
        parts.push_back(QString::number((double)var.value.f32v[i], 'g', 6));
        break;
      case VarType::SInt: parts.push_back(QString::number(var.value.s32v[i])); break;
      case VarType::UInt: parts.push_back(QString::number(var.value.u32v[i])); break;
      case VarType::Bool: parts.push_back(var.value.u32v[i] ? lit("true") : lit("false")); break;
      default: parts.push_back(QString::number((double)var.value.f32v[i], 'g', 6)); break;
    }
  }

  if(parts.size() == 1)
    return parts[0];
  return lit("(") + parts.join(lit(", ")) + lit(")");
}

// ---- private helpers --------------------------------------------------------

int PixelDebugSyncManager::findGroupIndex(uint32_t groupId) const
{
  for(int i = 0; i < m_Groups.size(); i++)
  {
    if(m_Groups[i].id == groupId)
      return i;
  }
  return -1;
}

bool PixelDebugSyncManager::varsAreDivergent(const ShaderVariable &a, const ShaderVariable &b,
                                             float threshold)
{
  // Unknown type (e.g. DXBC temp registers) — treat as Float for all comparisons
  VarType typeA = (a.type == VarType::Unknown) ? VarType::Float : a.type;
  VarType typeB = (b.type == VarType::Unknown) ? VarType::Float : b.type;

  if(a.rows != b.rows || a.columns != b.columns || typeA != typeB)
    return true;

  uint32_t count = (uint32_t)a.rows * (uint32_t)a.columns;
  VarType type = typeA;

  switch(type)
  {
    case VarType::Double:
      for(uint32_t i = 0; i < count; i++)
      {
        if(fabs(a.value.f64v[i] - b.value.f64v[i]) > (double)threshold)
          return true;
      }
      break;
    case VarType::Half:
      for(uint32_t i = 0; i < count; i++)
      {
        if(fabsf((float)a.value.f16v[i] - (float)b.value.f16v[i]) > threshold)
          return true;
      }
      break;
    case VarType::Float:
      for(uint32_t i = 0; i < count; i++)
      {
        if(fabsf(a.value.f32v[i] - b.value.f32v[i]) > threshold)
          return true;
      }
      break;
    default:
      // Integer and boolean types use exact comparison
      for(uint32_t i = 0; i < count; i++)
      {
        if(a.value.u32v[i] != b.value.u32v[i])
          return true;
      }
      break;
  }

  return false;
}

// ---- IPixelDebugSyncManager (Python/agent-facing interface) -----------------

rdcarray<uint32_t> PixelDebugSyncManager::GetAllGroupIds() const
{
  rdcarray<uint32_t> ids;
  for(const SyncGroup &g : m_Groups)
    ids.push_back(g.id);
  return ids;
}

SyncGroupInfo PixelDebugSyncManager::GetGroupInfo(uint32_t groupId) const
{
  const SyncGroup *g = getGroup(groupId);
  if(!g)
    return SyncGroupInfo();

  SyncGroupInfo info;
  info.id = g->id;
  info.name = rdcstr(g->name.toUtf8().constData());
  info.viewerCount = (uint32_t)g->viewers.size();
  info.threshold = g->threshold;
  info.ignoreIntDivergence = g->ignoreIntDivergence;
  info.autoBreakOnDivergence = g->autoBreakOnDivergence;
  info.autoBreakOnVarDivergence = g->autoBreakOnVarDivergence;
  return info;
}

rdcarray<SyncVarDiff> PixelDebugSyncManager::ComputeDiffs(uint32_t groupId) const
{
  QList<VarDiff> qtDiffs = computeDiffs(groupId);

  rdcarray<SyncVarDiff> result;
  result.reserve(qtDiffs.size());

  for(const VarDiff &vd : qtDiffs)
  {
    SyncVarDiff out;
    out.path = rdcstr(vd.path.toUtf8().constData());
    for(const ShaderVariable &v : vd.values)
      out.values.push_back(v);
    for(bool p : vd.present)
      out.present.push_back(p ? 1u : 0u);
    out.divergent = vd.divergent;
    for(bool c : vd.componentDivergent)
      out.componentDivergent.push_back(c ? 1u : 0u);
    result.push_back(out);
  }

  return result;
}

bool PixelDebugSyncManager::HasBranchDivergence(uint32_t groupId) const
{
  return hasBranchDivergence(groupId);
}

void PixelDebugSyncManager::SetThreshold(uint32_t groupId, float threshold)
{
  setThreshold(groupId, threshold);
}

void PixelDebugSyncManager::SetIgnoreIntDivergence(uint32_t groupId, bool ignore)
{
  setIgnoreIntDivergence(groupId, ignore);
}

rdcstr PixelDebugSyncManager::FormatVarValue(const ShaderVariable &var) const
{
  return rdcstr(formatVarValue(var).toUtf8().constData());
}

bool PixelDebugSyncManager::VarsAreDivergent(const ShaderVariable &a, const ShaderVariable &b,
                                             float threshold) const
{
  return varsAreDivergent(a, b, threshold);
}

// ---- private helpers --------------------------------------------------------

void PixelDebugSyncManager::collectLeafPaths(const ShaderVariable &var, const QString &prefix,
                                             QList<QString> &paths)
{
  if(var.members.empty())
  {
    // Only track numeric/boolean variables; skip resource handles
    switch(var.type)
    {
      case VarType::ReadOnlyResource:
      case VarType::ReadWriteResource:
      case VarType::Sampler: return;
      default: break;
    }
    paths.push_back(prefix);
  }
  else
  {
    for(const ShaderVariable &member : var.members)
      collectLeafPaths(member, prefix + lit(".") + QString(member.name), paths);
  }
}

ShaderVariable PixelDebugSyncManager::findVarByPath(const QList<ShaderVariable> &vars,
                                                    const QString &path)
{
  QStringList parts = path.split(lit("."));
  if(parts.empty())
    return ShaderVariable();

  const ShaderVariable *cur = NULL;
  for(const ShaderVariable &v : vars)
  {
    if(QString(v.name) == parts[0])
    {
      cur = &v;
      break;
    }
  }

  if(!cur)
    return ShaderVariable();

  for(int i = 1; i < parts.size(); i++)
  {
    const ShaderVariable *next = NULL;
    for(const ShaderVariable &m : cur->members)
    {
      if(QString(m.name) == parts[i])
      {
        next = &m;
        break;
      }
    }
    if(!next)
      return ShaderVariable();
    cur = next;
  }

  return *cur;
}
