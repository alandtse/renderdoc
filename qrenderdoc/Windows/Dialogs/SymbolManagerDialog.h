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

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

class QLineEdit;
class QTableWidget;
class QPushButton;

class SymbolManagerDialog : public QFrame, public ISymbolManager, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit SymbolManagerDialog(ICaptureContext &ctx, QWidget *parent = NULL);
  ~SymbolManagerDialog();

  // ISymbolManager
  QWidget *Widget() override { return this; }
  void Refresh() override { refresh(); }

  // ICaptureViewer
  void OnCaptureLoaded() override {}
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t) override {}
  void OnEventChanged(uint32_t) override {}

private slots:
  void loadPDB();
  void toggleIgnore();
  void selectionChanged();
  void filterChanged(const QString &text);
  void tableContextMenu(const QPoint &pos);

private:
  void refresh();
  void applyFilter();
  void updateButtonStates();
  void applyHeaderResizeModes();

  ICaptureContext &m_Ctx;
  QLineEdit *m_SearchBox;
  QTableWidget *m_Table;
  QPushButton *m_LoadButton;
  QPushButton *m_IgnoreButton;
  QPushButton *m_RefreshButton;
};
