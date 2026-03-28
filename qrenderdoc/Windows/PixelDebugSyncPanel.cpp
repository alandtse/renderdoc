/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PixelDebugSyncPanel.h"
#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "PixelDebugSyncManager.h"
#include "ShaderViewer.h"

// Fixed column indices in the diff table
static const int COL_VAR = 0;      // variable name / viewer label
static const int COL_VAL = 1;      // value for that viewer row
static const int COL_DELTA = 2;    // delta vs reference viewer (empty for the reference row)

static const QColor s_DivergentColor(255, 80, 80, 60);

PixelDebugSyncPanel::PixelDebugSyncPanel(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), m_Ctx(ctx)
{
  m_Manager = PixelDebugSyncManager::instance();

  // ---- top toolbar row ----
  QHBoxLayout *toolRow = new QHBoxLayout;
  toolRow->setContentsMargins(0, 0, 0, 0);
  toolRow->setSpacing(4);

  toolRow->addWidget(new QLabel(tr("Threshold:"), this));

  m_ThresholdSpin = new QDoubleSpinBox(this);
  m_ThresholdSpin->setDecimals(6);
  m_ThresholdSpin->setSingleStep(0.001);
  m_ThresholdSpin->setMinimum(0.0);
  m_ThresholdSpin->setMaximum(1e6);
  m_ThresholdSpin->setValue(0.001);
  m_ThresholdSpin->setToolTip(
      tr("Maximum absolute difference allowed between float values before they are flagged as "
         "divergent"));
  toolRow->addWidget(m_ThresholdSpin);

  toolRow->addSpacing(12);
  m_IgnoreIntCheck = new QCheckBox(tr("Ignore int/bool divergence"), this);
  m_IgnoreIntCheck->setChecked(false);
  m_IgnoreIntCheck->setToolTip(
      tr("Do not flag integer or boolean variable differences as divergence (useful when shaders "
         "use integer counters or IDs that are expected to differ between pixels)"));
  toolRow->addWidget(m_IgnoreIntCheck);

  toolRow->addSpacing(12);
  m_AutoBreakCheck = new QCheckBox(tr("Break on branch divergence"), this);
  m_AutoBreakCheck->setChecked(true);
  m_AutoBreakCheck->setToolTip(
      tr("When running forward, pause all viewers when they are about to execute different "
         "instructions"));
  toolRow->addWidget(m_AutoBreakCheck);

  m_AutoBreakVarCheck = new QCheckBox(tr("Break on variable divergence"), this);
  m_AutoBreakVarCheck->setChecked(false);
  m_AutoBreakVarCheck->setToolTip(tr(
      "When running forward, pause all viewers when any variable value becomes newly divergent"));
  toolRow->addWidget(m_AutoBreakVarCheck);

  toolRow->addStretch();

  // ---- step controls row ----
  QHBoxLayout *stepRow = new QHBoxLayout;
  stepRow->setContentsMargins(0, 0, 0, 0);
  stepRow->setSpacing(4);

  m_StepBackBtn = new QPushButton(tr("<< Step Back (all)"), this);
  m_StepBackBtn->setToolTip(tr("Step all viewers in this group backwards by one instruction"));
  stepRow->addWidget(m_StepBackBtn);

  m_StepFwdBtn = new QPushButton(tr("Step Fwd (all) >>"), this);
  m_StepFwdBtn->setToolTip(tr("Step all viewers in this group forwards by one instruction"));
  stepRow->addWidget(m_StepFwdBtn);

  m_RunFwdBtn = new QPushButton(tr(">> Run Fwd (all)"), this);
  m_RunFwdBtn->setToolTip(tr("Run all viewers forward to the next breakpoint or end of shader"));
  stepRow->addWidget(m_RunFwdBtn);

  stepRow->addStretch();

  // ---- divergence warning ----
  m_DivergenceLabel = new QLabel(this);
  m_DivergenceLabel->setStyleSheet(
      lit("QLabel { background-color: #AA4400; color: white; padding: 4px; border-radius: 3px; }"));
  m_DivergenceLabel->setWordWrap(true);
  m_DivergenceLabel->hide();

  // ---- diff table ----
  m_DiffTree = new QTreeWidget(this);
  m_DiffTree->setUniformRowHeights(true);
  m_DiffTree->setRootIsDecorated(true);
  m_DiffTree->setColumnCount(3);
  m_DiffTree->setHeaderLabels({tr("Variable"), tr("Value"), tr("Delta")});
  m_DiffTree->header()->setSectionResizeMode(COL_VAR, QHeaderView::ResizeToContents);
  m_DiffTree->header()->setSectionResizeMode(COL_VAL, QHeaderView::Stretch);
  m_DiffTree->header()->setSectionResizeMode(COL_DELTA, QHeaderView::Stretch);

  // ---- main layout ----
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);
  mainLayout->addLayout(toolRow);
  mainLayout->addLayout(stepRow);
  mainLayout->addWidget(m_DivergenceLabel);
  mainLayout->addWidget(m_DiffTree, 1);

  setWindowTitle(tr("Pixel Debug Sync"));

  // ---- signal/slot wiring ----
  QObject::connect(m_Manager, &PixelDebugSyncManager::groupListChanged, this,
                   &PixelDebugSyncPanel::onGroupListChanged);
  QObject::connect(m_Manager, &PixelDebugSyncManager::groupUpdated, this,
                   &PixelDebugSyncPanel::onGroupUpdated);
  QObject::connect(m_Manager, &PixelDebugSyncManager::stepCompleted, this,
                   &PixelDebugSyncPanel::onStepCompleted);
  QObject::connect(m_Manager, &PixelDebugSyncManager::branchDivergenceDetected, this,
                   &PixelDebugSyncPanel::onBranchDivergence);

  QObject::connect(m_ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &PixelDebugSyncPanel::onThresholdChanged);
  QObject::connect(m_IgnoreIntCheck, &QCheckBox::toggled, this,
                   &PixelDebugSyncPanel::onIgnoreIntChanged);
  QObject::connect(m_AutoBreakCheck, &QCheckBox::toggled, this,
                   &PixelDebugSyncPanel::onAutoBreakChanged);
  QObject::connect(m_AutoBreakVarCheck, &QCheckBox::toggled, this,
                   &PixelDebugSyncPanel::onAutoBreakVarChanged);
  QObject::connect(m_StepBackBtn, &QPushButton::clicked, this,
                   &PixelDebugSyncPanel::onStepAllBackward);
  QObject::connect(m_StepFwdBtn, &QPushButton::clicked, this, &PixelDebugSyncPanel::onStepAllForward);
  QObject::connect(m_RunFwdBtn, &QPushButton::clicked, this, &PixelDebugSyncPanel::onRunAllForward);

  m_Ctx.AddCaptureViewer(this);
}

PixelDebugSyncPanel::~PixelDebugSyncPanel()
{
  m_Ctx.RemoveCaptureViewer(this);
  m_Ctx.BuiltinWindowClosed(this);
}

// ---- ICaptureViewer --------------------------------------------------------

void PixelDebugSyncPanel::OnCaptureLoaded()
{
  onGroupListChanged();
}

void PixelDebugSyncPanel::OnCaptureClosed()
{
  m_ActiveGroupId = ~0U;
  m_DiffTree->clear();
  setDivergenceWarning(false);
}

// ---- private slots ---------------------------------------------------------

void PixelDebugSyncPanel::onGroupListChanged()
{
  // Clear the active group pointer if its group no longer exists.
  if(m_ActiveGroupId != ~0U && !m_Manager->allGroupIds().contains(m_ActiveGroupId))
    m_ActiveGroupId = ~0U;

  uint32_t groupId = selectedGroupId();
  if(groupId == ~0U)
  {
    m_DiffTree->clear();
    setDivergenceWarning(false);
    return;
  }

  const SyncGroup *g = m_Manager->getGroup(groupId);
  if(!g)
    return;

  m_ThresholdSpin->blockSignals(true);
  m_ThresholdSpin->setValue((double)g->threshold);
  m_ThresholdSpin->blockSignals(false);

  m_IgnoreIntCheck->blockSignals(true);
  m_IgnoreIntCheck->setChecked(g->ignoreIntDivergence);
  m_IgnoreIntCheck->blockSignals(false);

  m_AutoBreakCheck->blockSignals(true);
  m_AutoBreakCheck->setChecked(g->autoBreakOnDivergence);
  m_AutoBreakCheck->blockSignals(false);

  m_AutoBreakVarCheck->blockSignals(true);
  m_AutoBreakVarCheck->setChecked(g->autoBreakOnVarDivergence);
  m_AutoBreakVarCheck->blockSignals(false);

  refreshDiffTable(groupId);
}

void PixelDebugSyncPanel::onGroupUpdated(uint32_t groupId)
{
  if(groupId == selectedGroupId())
    onGroupListChanged();
}

void PixelDebugSyncPanel::onStepCompleted(uint32_t groupId)
{
  // Track the most recently active group so the panel follows the active debugging session.
  m_ActiveGroupId = groupId;
  setDivergenceWarning(false);
  refreshDiffTable(groupId);
}

void PixelDebugSyncPanel::onBranchDivergence(uint32_t groupId, uint32_t instrA, uint32_t instrB)
{
  if(groupId != selectedGroupId())
    return;

  setDivergenceWarning(
      true,
      tr("[!] Branch divergence: viewers are at instructions %1 vs %2").arg(instrA).arg(instrB));
}

void PixelDebugSyncPanel::onThresholdChanged(double value)
{
  uint32_t groupId = selectedGroupId();
  if(groupId != ~0U)
    m_Manager->setThreshold(groupId, (float)value);
}

void PixelDebugSyncPanel::onIgnoreIntChanged(bool checked)
{
  uint32_t groupId = selectedGroupId();
  if(groupId != ~0U)
    m_Manager->setIgnoreIntDivergence(groupId, checked);
}

void PixelDebugSyncPanel::onAutoBreakChanged(bool checked)
{
  uint32_t groupId = selectedGroupId();
  if(groupId != ~0U)
    m_Manager->setAutoBreakOnDivergence(groupId, checked);
}

void PixelDebugSyncPanel::onAutoBreakVarChanged(bool checked)
{
  uint32_t groupId = selectedGroupId();
  if(groupId != ~0U)
    m_Manager->setAutoBreakOnVarDivergence(groupId, checked);
}

void PixelDebugSyncPanel::onStepAllBackward()
{
  uint32_t groupId = selectedGroupId();
  const SyncGroup *g = m_Manager->getGroup(groupId);
  if(!g || g->viewers.empty())
    return;

  // Step the first viewer; the sync manager will drive the rest
  uint32_t cur = g->viewers[0]->CurrentStep();
  g->viewers[0]->SetCurrentStep(cur > 0 ? cur - 1 : 0);
}

void PixelDebugSyncPanel::onStepAllForward()
{
  uint32_t groupId = selectedGroupId();
  const SyncGroup *g = m_Manager->getGroup(groupId);
  if(!g || g->viewers.empty())
    return;

  // Step the first viewer; the sync manager will drive the rest
  g->viewers[0]->SetCurrentStep(g->viewers[0]->CurrentStep() + 1);
}

void PixelDebugSyncPanel::onRunAllForward()
{
  uint32_t groupId = selectedGroupId();
  const SyncGroup *g = m_Manager->getGroup(groupId);
  if(!g || g->viewers.empty())
    return;

  g->viewers[0]->RunForward();
}

// ---- private helpers -------------------------------------------------------

uint32_t PixelDebugSyncPanel::selectedGroupId() const
{
  QList<uint32_t> ids = m_Manager->allGroupIds();
  if(ids.isEmpty())
    return ~0U;
  // Follow the group that most recently had a step; fall back to the first available group.
  if(m_ActiveGroupId != ~0U && ids.contains(m_ActiveGroupId))
    return m_ActiveGroupId;
  return ids.first();
}

void PixelDebugSyncPanel::refreshDiffTable(uint32_t groupId)
{
  const SyncGroup *g = m_Manager->getGroup(groupId);
  if(!g || g->viewers.size() < 2)
  {
    m_DiffTree->clear();
    return;
  }

  QList<VarDiff> diffs = m_Manager->computeDiffs(groupId);

  m_DiffTree->clear();
  m_DiffTree->setUpdatesEnabled(false);

  for(const VarDiff &diff : diffs)
  {
    if(!diff.divergent)
      continue;

    // Resolve the debug register path to a high-level source variable name using viewer[0].
    QString displayName = g->viewers[0]->sourceNameForDebugPath(diff.path);
    if(displayName.isEmpty())
      displayName = diff.path;

    QTreeWidgetItem *parent = new QTreeWidgetItem(m_DiffTree);
    parent->setText(COL_VAR, displayName);
    parent->setBackground(COL_VAR, s_DivergentColor);
    parent->setExpanded(true);

    for(int vi = 0; vi < g->viewers.size(); vi++)
    {
      ShaderViewer *viewer = g->viewers[vi];
      QString serial = m_Manager->serialLabelForViewer(groupId, viewer);
      QString viewerLabel = serial.isEmpty() ? tr("Viewer %1").arg(vi + 1) : serial;
      if(vi == 0)
        viewerLabel += tr(" (ref)");

      QString value;
      if(vi < diff.values.size() && diff.present[vi])
        value = PixelDebugSyncManager::formatVarValue(diff.values[vi]);
      else
        value = lit("\u2014");

      QString delta;
      if(vi == 0)
      {
        delta = lit("\u2014");
      }
      else if(diff.present[0] && vi < diff.values.size() && diff.present[vi])
      {
        const ShaderVariable &a = diff.values[0];
        const ShaderVariable &b = diff.values[vi];
        VarType typeA = (a.type == VarType::Unknown) ? VarType::Float : a.type;
        if((typeA == VarType::Float || typeA == VarType::Half || typeA == VarType::Double) &&
           a.rows == b.rows && a.columns == b.columns)
        {
          uint32_t count = (uint32_t)a.rows * (uint32_t)a.columns;
          QStringList parts;
          for(uint32_t c = 0; c < count; c++)
          {
            float d = b.value.f32v[c] - a.value.f32v[c];
            parts.push_back(QString::number((double)d, 'g', 4));
          }
          delta = (parts.size() == 1) ? parts[0] : lit("(") + parts.join(lit(", ")) + lit(")");
        }
        else
        {
          delta = lit("\u2014");
        }
      }
      else
      {
        delta = lit("\u2014");
      }

      QTreeWidgetItem *child = new QTreeWidgetItem(parent);
      child->setText(COL_VAR, viewerLabel);
      child->setText(COL_VAL, value);
      child->setText(COL_DELTA, delta);

      // Highlight non-reference rows that are divergent
      if(vi > 0)
      {
        child->setBackground(COL_VAL, s_DivergentColor);
        child->setBackground(COL_DELTA, s_DivergentColor);
      }
    }
  }

  m_DiffTree->setUpdatesEnabled(true);
}

void PixelDebugSyncPanel::setDivergenceWarning(bool show, const QString &msg)
{
  if(show)
  {
    m_DivergenceLabel->setText(msg);
    m_DivergenceLabel->show();
  }
  else
  {
    m_DivergenceLabel->hide();
  }
}
