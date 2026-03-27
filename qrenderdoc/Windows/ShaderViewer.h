/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2026 Baldur Karlsson
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
#include <QSemaphore>
#include <QSet>
#include <QStyledItemDelegate>
#include "Code/Interface/QRDInterface.h"
#include "PixelDebugSyncManager.h"

namespace Ui
{
class ShaderViewer;
}

class RDTreeWidget;
class RDTreeWidgetItem;
class QLabel;
struct ShaderDebugTrace;
struct ShaderReflection;
class ScintillaEdit;
class FindReplace;
class QTableWidgetItem;
class QKeyEvent;
class QMouseEvent;
class QComboBox;
class QTextEdit;
class QToolButton;

// from Scintilla
typedef intptr_t sptr_t;

enum class VariableCategory
{
  Unknown,
  Inputs,
  Constants,
  ReadOnlyResource,
  ReadWriteResource,
  Variables,
  ByString,
};

enum class AccessedResourceView
{
  SortByStep,
  SortByResource,
};

struct AccessedResourceData
{
  ShaderVariable resource;
  rdcarray<size_t> steps;
};

enum class WatchVarState : int
{
  Invalid = 0,
  Valid = 1,
  Stale = 2,
};

struct VariableTag
{
  VariableTag() = default;
  VariableTag(DebugVariableType type, rdcstr name)
  {
    debugVarType = type;
    absoluteRefPath = name;
  }
  uint32_t offset = 0;

  WatchVarState state = WatchVarState::Invalid;

  bool matrix = false;
  bool expanded = false;
  bool modified = false;
  int updateID = 0;
  bool globalSourceVar = false;
  int32_t sourceVarIdx = -1;

  DebugVariableType debugVarType = DebugVariableType::Undefined;
  rdcstr absoluteRefPath;
};

struct ResourceReference
{
  ResourceReference() {}
  ResourceReference(ShaderBindIndex bp) : bind(bp), directAccess(false) {}
  ResourceReference(ShaderDirectAccess acc) : access(acc), directAccess(true) {}
  union
  {
    ShaderBindIndex bind;
    ShaderDirectAccess access;
  };
  bool directAccess;
};

class ShaderViewer : public QFrame, public IShaderViewer, public ICaptureViewer
{
  Q_OBJECT

public:
  typedef std::function<void(ShaderViewer *viewer, bool closed)> ModifyCallback;

  static ShaderViewer *LoadEditor(ICaptureContext &ctx, QVariantMap data,
                                  IShaderViewer::SaveCallback saveCallback,
                                  IShaderViewer::RevertCallback revertCallback,
                                  ModifyCallback modifyCallback, QWidget *parent);
  QVariantMap SaveEditor();

  static ShaderViewer *EditShader(ICaptureContext &ctx, ResourceId id, ShaderStage stage,
                                  const QString &entryPoint, const rdcstrpairs &files,
                                  KnownShaderTool knownTool, ShaderEncoding shaderEncoding,
                                  ShaderCompileFlags flags, IShaderViewer::SaveCallback saveCallback,
                                  IShaderViewer::RevertCallback revertCallback,
                                  ModifyCallback modifyCallback, QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->m_SaveCallback = saveCallback;
    ret->m_RevertCallback = revertCallback;
    ret->m_ModifyCallback = modifyCallback;
    ret->editShader(id, stage, entryPoint, files, knownTool, shaderEncoding, flags);
    return ret;
  }

  static IShaderViewer *DebugShader(ICaptureContext &ctx, const ShaderReflection *shader,
                                    ResourceId pipeline, ShaderDebugTrace *trace,
                                    const QString &debugContext, QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->debugShader(shader, pipeline, trace, debugContext);
    return ret;
  }

  static IShaderViewer *ViewShader(ICaptureContext &ctx, const ShaderReflection *shader,
                                   ResourceId pipeline, QWidget *parent)
  {
    return DebugShader(ctx, shader, pipeline, NULL, QString(), parent);
  }

  ~ShaderViewer();

  // IShaderViewer
  virtual QWidget *Widget() override { return this; }
  virtual uint32_t CurrentStep() override;
  virtual void SetCurrentStep(uint32_t step) override;

  virtual void ToggleBreakpointOnInstruction(int32_t instruction = -1) override;
  virtual void ToggleBreakpointOnDisassemblyLine(int32_t disassemblyLine) override;
  virtual void RunForward() override;

  virtual void ShowErrors(const rdcstr &errors) override;

  virtual void AddWatch(const rdcstr &variable) override;

  virtual rdcstrpairs GetCurrentFileContents() override;

  // Sync group support (used by PixelDebugSyncManager / PixelDebugSyncPanel)

  // Join the given group on the singleton sync manager. Removes the viewer from any prior group.
  void joinSyncGroup(uint32_t groupId);
  // Leave the current sync group, if any.
  void leaveSyncGroup();
  bool isInSyncGroup() const { return m_SyncGroupId != ~0U; }

  // Called by PixelDebugSyncManager to drive this viewer to a specific step without re-notifying
  // the manager (re-entry guard).
  void syncStep(uint32_t stepIndex);

  // Called by PixelDebugSyncManager to drive this viewer's view mode without re-notifying
  // the manager (re-entry guard).
  void syncDebugMode(bool sourceDebugging, bool intView, bool floatView);

  // Accessors used by PixelDebugSyncManager for comparison and divergence detection.
  const QList<ShaderVariable> &GetCurrentVariables() const { return m_Variables; }
  uint32_t GetCurrentInstruction() const;
  QString debugContext() const { return m_DebugContext; }
  uint32_t debugEventId() const { return m_DebugEventId; }
  // The ResourceId of the shader being debugged (for validating sync group membership).
  ResourceId shaderResourceId() const
  {
    return m_ShaderDetails ? m_ShaderDetails->resourceId : ResourceId();
  }
  bool isSourceDebugging();
  bool isIntView() const;
  bool isFloatView() const;

  // Given a debug variable path (e.g. "r0"), returns the first matching high-level source variable
  // name from the current instruction's sourceVars or the trace's global sourceVars.
  // Returns an empty string if no mapping is found.
  QString sourceNameForDebugPath(const QString &path) const;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

private slots:
  // automatic slots
  void on_findReplace_clicked();
  void on_refresh_clicked();
  void on_unrefresh_clicked();
  void on_resetEdits_clicked();
  void on_intView_clicked();
  void on_floatView_clicked();
  void on_debugToggle_clicked();
  void on_toggleLog_clicked();

  void on_resources_sortByStep_clicked();
  void on_resources_sortByResource_clicked();

  void on_watch_itemChanged(RDTreeWidgetItem *item, int column);

  // manual slots
  void readonly_keyPressed(QKeyEvent *event);
  void editable_keyPressed(QKeyEvent *event);
  void debug_contextMenu(const QPoint &pos);
  void variables_contextMenu(const QPoint &pos);
  void accessedResources_contextMenu(const QPoint &pos);
  void disassembly_buttonReleased(QMouseEvent *event);
  void disassemble_typeChanged(int index);
  void watch_keyPress(QKeyEvent *event);
  void performFind();
  void performFindAll();
  void resultsDoubleClick(int position, int line);
  void performReplace();
  void performReplaceAll();

  void snippet_constants();
  void snippet_samplers();
  void snippet_resources();

  void disasm_tooltipShow(int x, int y);
  void disasm_tooltipHide(int x, int y);

private:
  explicit ShaderViewer(ICaptureContext &ctx, QWidget *parent = 0);
  void editShader(ResourceId id, ShaderStage stage, const QString &entryPoint,
                  const rdcstrpairs &files, KnownShaderTool knownTool,
                  ShaderEncoding shaderEncoding, ShaderCompileFlags flags);
  void debugShader(const ShaderReflection *shader, ResourceId pipeline, ShaderDebugTrace *trace,
                   const QString &debugContext);

  bool eventFilter(QObject *watched, QEvent *event) override;

  QAction *MakeExecuteAction(QString name, const QIcon &icon, QString tooltip, QKeySequence shortcut);

  void MarkModification();

  void ConfigureBookmarkMenu();
  void UpdateBookmarkMenu(QMenu *menu, QAction *nextAction, QAction *prevAction,
                          QAction *clearAction);

  void PopulateCompileTools();
  void PopulateCompileToolParameters();
  bool ProcessIncludeDirectives(QString &source, const rdcstrpairs &files,
                                rdcarray<rdcstr> &allIncluded, const rdcarray<rdcstr> &exclude = {});

  void updateWindowTitle();
  void gotoSourceDebugging();
  void gotoDisassemblyDebugging();

  void insertSnippet(const QString &text);

  void showVariableTooltip(QString name);
  void updateVariableTooltip();
  void hideVariableTooltip();
  QString syncDiffTooltipSuffix(const SourceVariableMapping &mapping);

  void cacheResources();

  ShaderEncoding currentEncoding();

  void ToggleBookmark();
  void NextBookmark();
  void PreviousBookmark();
  void ClearAllBookmarks();
  bool HasBookmarks();

  void SetFindTextFromCurrentWord();

  QString m_TooltipVarPath;
  int m_TooltipVarIndex = -1;
  int m_TooltipMember = -1;
  QPoint m_TooltipPos;

  Ui::ShaderViewer *ui;
  ICaptureContext &m_Ctx;
  const ShaderReflection *m_ShaderDetails = NULL;
  bool m_CustomShader = false;
  ResourceId m_EditingShader;
  ShaderCompileFlags m_Flags;
  QList<ShaderEncoding> m_Encodings;
  ShaderStage m_Stage;
  QString m_DebugContext;
  ResourceId m_Pipeline;
  rdcarray<rdcstr> m_PipelineTargets;
  ScintillaEdit *m_DisassemblyView = NULL;
  QFrame *m_DisassemblyToolbar = NULL;
  QWidget *m_DisassemblyFrame = NULL;
  QComboBox *m_DisassemblyType = NULL;
  ScintillaEdit *m_Errors = NULL;
  ScintillaEdit *m_FindResults = NULL;
  QList<ScintillaEdit *> m_Scintillas;

  // a map, from a source location to a list of instruction blocks with that location.
  // not every instruction with the instruction will be mapped to, only the first in each contiguous
  // run
  QMap<LineColumnInfo, rdcarray<uint32_t>> m_Location2Inst;

  // a vector for the disassembly with the instruction index for each disassembly line
  QVector<int32_t> m_AsmLine2Inst;

  ScintillaEdit *m_CurInstructionScintilla = NULL;
  QList<ScintillaEdit *> m_FileScintillas;

  FindReplace *m_FindReplace;

  struct FindState
  {
    // hash identifies when the search has changed
    QString hash;

    // the range identified when the search first occurred (for incremental find/replace)
    sptr_t start = 0;
    sptr_t end = 0;

    // the current offset where to search from next time, relative to above range
    sptr_t offset = 0;

    // the last result
    QPair<int, int> prevResult;
  } m_FindState;

  SaveCallback m_SaveCallback;
  RevertCallback m_RevertCallback;
  ModifyCallback m_ModifyCallback;

  bool m_Modified = true;
  bool m_Saved = false;

  ShaderDebugTrace *m_Trace = NULL;
  size_t m_FirstSourceStateIdx = ~0U;
  rdcarray<ShaderDebugState> m_States;
  size_t m_CurrentStateIdx = 0;
  QList<ShaderVariable> m_Variables;
  uint32_t m_UpdateID = 1;
  QMap<QString, uint32_t> m_VariableLastUpdate;
  QList<rdcstr> m_VariablesChanged;

  // true when debugging while we're populating the initial trace. Lets us queue up commands and
  // process them once we've initialised properly
  bool m_DeferredInit = false;
  rdcarray<std::function<void(ShaderViewer *)>> m_DeferredCommands;

  bool m_ContextActive = false;

  QSemaphore m_BackgroundRunning;

  rdcarray<AccessedResourceData> m_AccessedResources;
  AccessedResourceView m_AccessedResourceView = AccessedResourceView::SortByResource;

  rdcarray<UsedDescriptor> m_ReadOnlyResources;
  rdcarray<UsedDescriptor> m_ReadWriteResources;
  QSet<QPair<int, uint32_t>> m_Breakpoints;
  bool m_TempBreakpoint = false;

  std::map<ShaderDirectAccess, rdcstr> m_LogicalBindNames;

  QList<QPair<ScintillaEdit *, int>> m_FindAllResults;

  static const int BOOKMARK_MAX_MENU_ENTRY_LENGTH = 40;    // max length of bookmark names in menu
  static const int BOOKMARK_MAX_MENU_ENTRY_COUNT = 30;     // max number of bookmarks listed in menu
  QMap<ScintillaEdit *, QList<sptr_t>> m_Bookmarks;

  QTextEdit *debugInfoLog = NULL;

  // The event ID that was current when this debug session was started, used for "go to debug event".
  uint32_t m_DebugEventId = 0;

  // Sync group tracking
  uint32_t m_SyncGroupId = ~0U;    // ~0U means not in any group
  bool m_SyncStepping = false;     // true while the sync manager is driving this viewer
  bool m_SyncMode = false;         // true while manager is driving display mode sync
  QToolButton *m_SyncBtn = NULL;

  // Divergence highlighting state
  QSet<QString> m_DivergentPaths;    // debug-var paths that differ across sync group viewers
  // Per-component divergence: maps debug-var path → QList<bool> (rows*columns entries).
  // Used so that source variables are only highlighted when their specific mapped component
  // diverges, not just because another component of the same register diverges.
  QMap<QString, QList<bool>> m_DivergentComponents;
  bool m_LastWasDivergent = false;    // previous-step divergence state, for edge detection
  QMetaObject::Connection m_SyncStepConnection;    // connection to stepCompleted signal

  // Embedded sync status label (shown as a docking tab when in a sync group)
  QWidget *m_SyncDiffPanel = NULL;
  QLabel *m_SyncStatusLabel = NULL;

  // Re-entry guard: prevents expansion-sync from triggering an infinite loop.
  bool m_SyncExpanding = false;
  QMetaObject::Connection m_SyncExpandedConnection;
  QMetaObject::Connection m_SyncCollapsedConnection;

  void updateSyncUI();
  void updateSyncButtonTooltip();
  void syncButtonContextMenu(const QPoint &pos);
  void updateSyncStatusLabel();

  // Apply an expansion state received from a peer viewer (re-entry-safe).
  void applySyncExpansion(const QSet<uint> &state);

  // Run forwards or backwards until any value or branch divergence is detected across the group.
  void runToDivergence(bool forward);
  // Bookmark the current instruction in the disassembly and source views (for divergence events).
  void addDivergenceBookmark();

  static const int CURRENT_MARKER = 0;
  static const int BREAKPOINT_MARKER = 2;
  static const int FINISHED_MARKER = 4;
  static const int BOOKMARK_MARKER = 6;

  static const int CURRENT_INDICATOR = 20;
  static const int FINISHED_INDICATOR = 21;

  static const int INDICATOR_FINDRESULT = 0;
  static const int INDICATOR_REGHIGHLIGHT = 1;
  static const int INDICATOR_FINDALLHIGHLIGHT = 2;

  QString targetName(const ShaderProcessingTool &disasm);

  void addFileList();

  ScintillaEdit *MakeEditor(const QString &name, const QString &text, int lang);
  void SetTextAndUpdateMargin0(ScintillaEdit *ret, const QString &text);

  ScintillaEdit *AddFileScintilla(const QString &name, const QString &text, ShaderEncoding encoding);

  ScintillaEdit *currentScintilla();
  ScintillaEdit *nextScintilla(ScintillaEdit *cur);

  int snippetPos();
  QString vulkanUBO();

  int instructionForDisassemblyLine(sptr_t line);

  bool IsFirstState() const;
  bool IsLastState() const;
  const ShaderDebugState &GetPreviousState() const;
  const ShaderDebugState &GetCurrentState() const;
  const ShaderDebugState &GetNextState() const;

  const InstructionSourceInfo &GetPreviousInstInfo() const;
  const InstructionSourceInfo &GetCurrentInstInfo() const;
  const InstructionSourceInfo &GetNextInstInfo() const;
  const InstructionSourceInfo &GetInstInfo(uint32_t instruction) const;
  const LineColumnInfo &GetCurrentLineInfo() const;

  void updateDebugState();
  void markWatchStale(RDTreeWidgetItem *item);
  bool updateWatchVariable(RDTreeWidgetItem *watchItem, const RDTreeWidgetItem *varItem,
                           const rdcstr &path, uint32_t swizzle, const SourceVariableMapping &mapping,
                           const ShaderVariable &var, QChar regcast);
  void updateWatchVariables();

  void updateAccessedResources();

  RDTreeWidgetItem *makeSourceVariableNode(const ShaderVariable &var, const rdcstr &debugVarPath,
                                           VariableTag baseTag);
  RDTreeWidgetItem *makeSourceVariableNode(const SourceVariableMapping &l, int globalVarIdx,
                                           int localVarIdx);
  RDTreeWidgetItem *makeDebugVariableNode(const ShaderVariable &v, rdcstr prefix);
  RDTreeWidgetItem *makeAccessedResourceNode(const ShaderVariable &v);

  bool HasChanged(rdcstr debugVarName) const;
  uint32_t CalcUpdateID(uint32_t prevID, rdcstr debugVarName) const;

  const ShaderVariable *GetDebugVariable(const DebugVariableReference &r);

  void ensureLineScrolled(ScintillaEdit *s, int i);

  void find(bool down);

  void updateEditState();

  enum StepMode
  {
    StepInto,
    StepOver,
    StepOut,
  };

  bool step(bool forward, StepMode mode);

  // Notify the sync manager that this viewer has moved to a new step. No-op when m_SyncGroupId is
  // unset or when m_SyncStepping is true (prevents re-entrant notifications).
  void notifySyncManager();

  void runToCursor(bool forward);
  void runTo(const rdcarray<uint32_t> &runToInstructions, bool forward, ShaderEvents condition);
  void runTo(uint32_t runToInstruction, bool forward, ShaderEvents condition = ShaderEvents::NoEvent);

  void runToResourceAccess(bool forward, VarType type, const ResourceReference &resRef);

  void applyBackwardsChange();
  void applyForwardsChange();

  QString stringRep(const ShaderVariable &var, uint32_t row = 0);
  QString samplerRep(const ShaderSampler &samp, uint32_t arrayElement, ResourceId id);
  void combineStructures(RDTreeWidgetItem *root, int skipPrefixLength = 0);
  void highlightMatchingVars(RDTreeWidgetItem *root, const QString varName,
                             const QColor highlightColor);

  QString getRegNames(const RDTreeWidgetItem *item, uint32_t swizzle, uint32_t child = ~0U);
  const RDTreeWidgetItem *evaluateVar(const RDTreeWidgetItem *item, uint32_t swizzle,
                                      ShaderVariable *var, SourceVariableMapping *mapping);
  const RDTreeWidgetItem *getVarFromPath(const rdcstr &path, const RDTreeWidgetItem *root,
                                         ShaderVariable *var, uint32_t *swizzle,
                                         SourceVariableMapping *mapping);
  const RDTreeWidgetItem *getVarFromPath(const rdcstr &path, ShaderVariable *var = NULL,
                                         uint32_t *swizzle = NULL,
                                         SourceVariableMapping *mapping = NULL);
};
