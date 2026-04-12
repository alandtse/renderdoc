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

#include "SymbolManagerDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include "Code/Interface/QRDInterface.h"
#include "Code/QRDUtils.h"

// Column indices
enum
{
  COL_MODULE = 0,
  COL_STATUS,
  COL_PDB_PATH,
  COL_REASON,
  COL_COUNT,
};

static QString statusToString(PDBStatus s)
{
  switch(s)
  {
    case PDBStatus::Loaded: return QApplication::tr("Loaded");
    case PDBStatus::ForceLoaded: return QApplication::tr("Force Loaded");
    case PDBStatus::Ignored: return QApplication::tr("Ignored");
    case PDBStatus::NotFound: return QApplication::tr("Not Found");
    case PDBStatus::Failed: return QApplication::tr("Failed");
    case PDBStatus::Skipped: return QApplication::tr("Skipped");
    default: return QApplication::tr("Unknown");
  }
}

static QColor statusColor(PDBStatus s)
{
  switch(s)
  {
    case PDBStatus::Loaded: return QColor(0x22, 0x8B, 0x22);         // forest green
    case PDBStatus::ForceLoaded: return QColor(0x00, 0x7A, 0xCC);    // blue — force-loaded flag
    case PDBStatus::NotFound: return QColor(0xCC, 0x55, 0x00);       // orange-red
    case PDBStatus::Failed: return QColor(0xAA, 0x00, 0x00);         // dark red
    case PDBStatus::Ignored:
    case PDBStatus::Skipped:
    default: return QColor(0x80, 0x80, 0x80);    // grey
  }
}

SymbolManagerDialog::SymbolManagerDialog(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), m_Ctx(ctx)
{
  setWindowTitle(tr("Symbol Manager"));

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);

  QLabel *hint = new QLabel(tr("PDB symbol load status for each module in the capture. "
                               "Hover a row for the full detail message. "
                               "Right-click any cell to copy."),
                            this);
  hint->setWordWrap(true);
  layout->addWidget(hint);

  m_SearchBox = new QLineEdit(this);
  m_SearchBox->setPlaceholderText(tr("Filter modules (searches all columns)..."));
  m_SearchBox->setClearButtonEnabled(true);
  layout->addWidget(m_SearchBox);

  m_Table = new QTableWidget(0, COL_COUNT, this);
  m_Table->setHorizontalHeaderLabels({tr("Module"), tr("Status"), tr("PDB Path"), tr("Details")});

  m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_Table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_Table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_Table->setAlternatingRowColors(true);
  m_Table->verticalHeader()->hide();
  m_Table->setSortingEnabled(true);
  m_Table->horizontalHeader()->setSortIndicatorShown(true);
  m_Table->setContextMenuPolicy(Qt::CustomContextMenu);
  applyHeaderResizeModes();
  layout->addWidget(m_Table);

  QHBoxLayout *buttons = new QHBoxLayout();

  m_LoadButton = new QPushButton(tr("Load PDB..."), this);
  m_LoadButton->setEnabled(false);
  m_LoadButton->setToolTip(
      tr("Browse for a PDB file to load for the selected module, bypassing GUID/age validation"));

  m_IgnoreButton = new QPushButton(tr("Ignore Module"), this);
  m_IgnoreButton->setEnabled(false);
  m_IgnoreButton->setToolTip(
      tr("Add or remove the selected module from the persistent ignore list"));

  m_RefreshButton = new QPushButton(tr("Refresh"), this);
  m_RefreshButton->setToolTip(tr("Reload the module status list"));

  buttons->addWidget(m_LoadButton);
  buttons->addWidget(m_IgnoreButton);
  buttons->addWidget(m_RefreshButton);
  buttons->addStretch(1);
  layout->addLayout(buttons);

  QObject::connect(m_Table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                   &SymbolManagerDialog::selectionChanged);
  QObject::connect(m_LoadButton, &QPushButton::clicked, this, &SymbolManagerDialog::loadPDB);
  QObject::connect(m_IgnoreButton, &QPushButton::clicked, this, &SymbolManagerDialog::toggleIgnore);
  QObject::connect(m_RefreshButton, &QPushButton::clicked, this, &SymbolManagerDialog::refresh);
  QObject::connect(m_SearchBox, &QLineEdit::textChanged, this, &SymbolManagerDialog::filterChanged);
  QObject::connect(m_Table, &QTableWidget::customContextMenuRequested, this,
                   &SymbolManagerDialog::tableContextMenu);
  QObject::connect(m_Table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
                   [this](int, Qt::SortOrder) { applyHeaderResizeModes(); });

  m_Ctx.AddCaptureViewer(this);
}

SymbolManagerDialog::~SymbolManagerDialog()
{
  m_Ctx.RemoveCaptureViewer(this);
}

void SymbolManagerDialog::OnCaptureClosed()
{
  m_Table->setSortingEnabled(false);
  m_Table->setRowCount(0);
  m_Table->setSortingEnabled(true);
  applyHeaderResizeModes();
  updateButtonStates();
}

// Qt resets QHeaderView section resize modes when setSortingEnabled() is toggled or when a sort
// completes. Connected to sortIndicatorChanged and called after setSortingEnabled(true).
void SymbolManagerDialog::applyHeaderResizeModes()
{
  QHeaderView *hdr = m_Table->horizontalHeader();
  hdr->setSectionResizeMode(QHeaderView::Stretch);
  hdr->setSectionResizeMode(COL_STATUS, QHeaderView::ResizeToContents);
}

void SymbolManagerDialog::refresh()
{
  ICaptureAccess *access = m_Ctx.Replay().GetCaptureAccess();
  if(!access)
  {
    m_Table->setSortingEnabled(false);
    m_Table->setRowCount(0);
    m_Table->setSortingEnabled(true);
    applyHeaderResizeModes();
    return;
  }

  rdcarray<ModuleStatus> statuses;

  // GetModuleStatuses must be called on the replay thread
  m_Ctx.Replay().BlockInvoke(
      [access, &statuses](IReplayController *) { statuses = access->GetModuleStatuses(); });

  // Disable sorting during population to avoid mid-insert re-sorts corrupting row order.
  // Re-apply resize modes afterwards because Qt resets them when sorting is toggled.
  m_Table->setSortingEnabled(false);
  m_Table->setRowCount(0);
  m_Table->setRowCount((int)statuses.size());

  for(int i = 0; i < (int)statuses.size(); i++)
  {
    const ModuleStatus &s = statuses[i];
    QColor col = statusColor(s.status);
    QString reason = QString::fromUtf8(s.statusReason.c_str());
    QString tooltip = reason.isEmpty() ? statusToString(s.status) : reason;

    auto makeItem = [&](const QString &text) {
      QTableWidgetItem *item = new QTableWidgetItem(text);
      item->setForeground(col);
      item->setToolTip(tooltip);
      return item;
    };

    m_Table->setItem(i, COL_MODULE, makeItem(QString::fromUtf8(s.moduleName.c_str())));
    m_Table->setItem(i, COL_STATUS, makeItem(statusToString(s.status)));
    m_Table->setItem(i, COL_PDB_PATH, makeItem(QString::fromUtf8(s.pdbPath.c_str())));
    m_Table->setItem(i, COL_REASON, makeItem(reason));
  }

  m_Table->setSortingEnabled(true);
  applyHeaderResizeModes();
  applyFilter();
  updateButtonStates();
}

void SymbolManagerDialog::applyFilter()
{
  QString filter = m_SearchBox->text().trimmed();
  for(int i = 0; i < m_Table->rowCount(); i++)
  {
    bool match = filter.isEmpty();
    if(!match)
    {
      for(int col = 0; col < COL_COUNT; col++)
      {
        QTableWidgetItem *item = m_Table->item(i, col);
        if(item && item->text().contains(filter, Qt::CaseInsensitive))
        {
          match = true;
          break;
        }
      }
    }
    m_Table->setRowHidden(i, !match);
    if(!match && m_Table->item(i, 0) && m_Table->item(i, 0)->isSelected())
      m_Table->clearSelection();
  }
  updateButtonStates();
}

void SymbolManagerDialog::filterChanged(const QString &)
{
  applyFilter();
}

void SymbolManagerDialog::updateButtonStates()
{
  QList<QTableWidgetItem *> sel = m_Table->selectedItems();
  if(sel.isEmpty())
  {
    m_LoadButton->setEnabled(false);
    m_IgnoreButton->setEnabled(false);
    return;
  }

  int row = sel.first()->row();
  QTableWidgetItem *statusItem = m_Table->item(row, COL_STATUS);
  if(!statusItem)
  {
    m_LoadButton->setEnabled(false);
    m_IgnoreButton->setEnabled(false);
    return;
  }

  QString statusText = statusItem->text();
  bool isLoaded = (statusText == tr("Loaded"));
  bool isForceLoaded = (statusText == tr("Force Loaded"));
  bool isSkipped = (statusText == tr("Skipped"));
  bool isIgnored = (statusText == tr("Ignored"));

  m_LoadButton->setEnabled(!isLoaded && !isForceLoaded);
  m_IgnoreButton->setEnabled(!isSkipped);
  m_IgnoreButton->setText(isIgnored ? tr("Un-ignore Module") : tr("Ignore Module"));
}

void SymbolManagerDialog::selectionChanged()
{
  updateButtonStates();
}

void SymbolManagerDialog::loadPDB()
{
  QList<QTableWidgetItem *> sel = m_Table->selectedItems();
  if(sel.isEmpty())
    return;

  int row = sel.first()->row();
  QTableWidgetItem *modItem = m_Table->item(row, COL_MODULE);
  if(!modItem)
    return;

  QString modName = modItem->text();

  // Derive the expected PDB filename from the module path so the dialog opens in the right
  // directory with the right default name — mirrors the automatic loader's behaviour.
  QString startPath;
  QFileInfo modInfo(modName);
  if(modInfo.isAbsolute())
    startPath = modInfo.absolutePath() + QStringLiteral("/") + modInfo.completeBaseName() +
                QStringLiteral(".pdb");

  QString pdbPath =
      RDDialog::getOpenFileName(this, tr("Locate PDB File"), startPath, tr("PDB Files (*.pdb)"));
  if(pdbPath.isEmpty())
    return;

  ICaptureAccess *access = m_Ctx.Replay().GetCaptureAccess();
  if(!access)
    return;

  bool ok = false;
  rdcstr mod = modName.toUtf8().constData();
  rdcstr pdb = pdbPath.toUtf8().constData();

  m_Ctx.Replay().BlockInvoke(
      [access, &mod, &pdb, &ok](IReplayController *) { ok = access->ForceLoadPDB(mod, pdb); });

  if(!ok)
  {
    RDDialog::critical(this, tr("Failed to load PDB"),
                       tr("Could not load '%1' for module:\n%2\n\n"
                          "The file may not be a valid PDB.")
                           .arg(pdbPath)
                           .arg(modName));
  }

  refresh();

  if(m_Ctx.HasAPIInspector())
    m_Ctx.GetAPIInspector()->Refresh();
}

void SymbolManagerDialog::toggleIgnore()
{
  QList<QTableWidgetItem *> sel = m_Table->selectedItems();
  if(sel.isEmpty())
    return;

  int row = sel.first()->row();
  QTableWidgetItem *modItem = m_Table->item(row, COL_MODULE);
  QTableWidgetItem *statusItem = m_Table->item(row, COL_STATUS);
  if(!modItem || !statusItem)
    return;

  ICaptureAccess *access = m_Ctx.Replay().GetCaptureAccess();
  if(!access)
    return;

  rdcstr mod = modItem->text().toUtf8().constData();
  bool isIgnored = (statusItem->text() == tr("Ignored"));

  m_Ctx.Replay().BlockInvoke([access, &mod, isIgnored](IReplayController *) {
    if(isIgnored)
      access->RemoveIgnore(mod);
    else
      access->AddIgnore(mod);
  });

  refresh();
}

void SymbolManagerDialog::tableContextMenu(const QPoint &pos)
{
  QTableWidgetItem *item = m_Table->itemAt(pos);
  if(!item)
    return;

  int row = item->row();

  QMenu menu(this);
  QAction *copyCell = menu.addAction(tr("Copy Cell"));
  QAction *copyRow = menu.addAction(tr("Copy Row"));
  menu.addSeparator();
  QAction *copyTooltip = menu.addAction(tr("Copy Full Detail"));

  QAction *chosen = menu.exec(m_Table->viewport()->mapToGlobal(pos));

  if(chosen == copyCell)
  {
    QApplication::clipboard()->setText(item->text());
  }
  else if(chosen == copyRow)
  {
    QStringList parts;
    for(int col = 0; col < COL_COUNT; col++)
    {
      QTableWidgetItem *colItem = m_Table->item(row, col);
      parts << (colItem ? colItem->text() : QString());
    }
    QApplication::clipboard()->setText(parts.join(QStringLiteral("\t")));
  }
  else if(chosen == copyTooltip)
  {
    QApplication::clipboard()->setText(item->toolTip());
  }
}
