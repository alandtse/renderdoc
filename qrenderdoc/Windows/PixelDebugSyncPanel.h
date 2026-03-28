/*
 * Copyright (C) 2026 Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

class PixelDebugSyncManager;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTreeWidget;

// Dockable panel that shows a side-by-side comparison of variable values across all viewers in a
// sync group, highlights divergent values, and reports branch divergence.
class PixelDebugSyncPanel : public QFrame, public IPixelDebugSyncPanel, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit PixelDebugSyncPanel(ICaptureContext &ctx, QWidget *parent = NULL);
  ~PixelDebugSyncPanel();

  // IPixelDebugSyncPanel
  QWidget *Widget() override { return this; }

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override {}

private slots:
  void onGroupListChanged();
  void onGroupUpdated(uint32_t groupId);
  void onStepCompleted(uint32_t groupId);
  void onBranchDivergence(uint32_t groupId, uint32_t instrA, uint32_t instrB);
  void onThresholdChanged(double value);
  void onAutoBreakChanged(bool checked);
  void onAutoBreakVarChanged(bool checked);
  void onIgnoreIntChanged(bool checked);
  void onStepAllBackward();
  void onStepAllForward();
  void onRunAllForward();

private:
  uint32_t selectedGroupId() const;
  void refreshDiffTable(uint32_t groupId);
  void setDivergenceWarning(bool show, const QString &msg = QString());

  ICaptureContext &m_Ctx;
  PixelDebugSyncManager *m_Manager = NULL;
  // The group that most recently had a step event; the panel displays this group.
  uint32_t m_ActiveGroupId = ~0U;

  QDoubleSpinBox *m_ThresholdSpin = NULL;
  QCheckBox *m_IgnoreIntCheck = NULL;
  QCheckBox *m_AutoBreakCheck = NULL;
  QCheckBox *m_AutoBreakVarCheck = NULL;
  QLabel *m_DivergenceLabel = NULL;
  QTreeWidget *m_DiffTree = NULL;
  QPushButton *m_StepBackBtn = NULL;
  QPushButton *m_StepFwdBtn = NULL;
  QPushButton *m_RunFwdBtn = NULL;
};
