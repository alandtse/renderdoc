/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
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

#include <QDir>
#include <QFrame>
#include <QMenu>
#include <QMouseEvent>
#include <QTime>
#include <QTimer>
#include "Code/Interface/QRDInterface.h"
#include "Code/SBSDetector.h"
#include "Code/SBSMapper.h"

namespace Ui
{
class TextureViewer;
}

class QLabel;
class RDTreeWidgetItem;
class ResourcePreview;
class ThumbnailStrip;
class TextureGoto;
class QFileSystemWatcher;
class TextureViewer;

struct Following
{
  FollowType Type;
  ShaderStage Stage;
  int index;
  uint32_t arrayEl;

  // this is only for QVariant compatibility and will generate an invalid Following instance! do not
  // use!
  Following();

  Following(const TextureViewer &tex, FollowType type, ShaderStage stage, uint32_t index,
            uint32_t arrayElement);
  Following(const Following &other);
  Following &operator=(const Following &other);

  bool operator==(const Following &o);
  bool operator!=(const Following &o);
  static void GetActionContext(ICaptureContext &ctx, bool &copy, bool &clear, bool &compute);

  int GetHighestMip(ICaptureContext &ctx);
  int GetFirstArraySlice(ICaptureContext &ctx);
  CompType GetTypeHint(ICaptureContext &ctx);

  ResourceId GetResourceId(ICaptureContext &ctx);
  Descriptor GetDescriptor(ICaptureContext &ctx, uint32_t arrayIdx);

  static rdcarray<Descriptor> GetOutputTargets(ICaptureContext &ctx);

  static Descriptor GetDepthTarget(ICaptureContext &ctx);
  static Descriptor GetDepthResolveTarget(ICaptureContext &ctx);
  static rdcarray<UsedDescriptor> GetReadWriteResources(ICaptureContext &ctx, ShaderStage stage,
                                                        bool onlyUsed);
  static rdcarray<UsedDescriptor> GetReadOnlyResources(ICaptureContext &ctx, ShaderStage stage,
                                                       bool onlyUsed);

  const ShaderReflection *GetReflection(ICaptureContext &ctx);
  static const ShaderReflection *GetReflection(ICaptureContext &ctx, ShaderStage stage);

private:
  const TextureViewer &tex;
};

struct TexSettings
{
  TexSettings()
  {
    displayType = 0;
    r = g = b = true;
    a = false;
    flip_y = false;
    depth = true;
    stencil = false;
    mip = 0;
    slice = 0;
    minrange = 0.0f;
    maxrange = 1.0f;
    typeCast = CompType::Typeless;
  }

  int displayType;    // RGBA, RGBM, YUV Decode, Custom
  QString customShader;
  bool r, g, b, a;
  bool flip_y;
  bool depth, stencil;
  int mip, slice;
  float minrange, maxrange;
  CompType typeCast;
};

class TextureViewer : public QFrame, public ITextureViewer, public ICaptureViewer
{
private:
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

  // Texture List
  enum class FilterType
  {
    None,
    Textures,
    RenderTargets,
    String
  };

public:
  explicit TextureViewer(ICaptureContext &ctx, QWidget *parent = 0);
  ~TextureViewer();

  // ITextureViewer
  QWidget *Widget() override { return this; }
  void ViewTexture(ResourceId ID, CompType typeCast, bool focus) override;
  void ViewFollowedResource(FollowType followType, ShaderStage stage, int32_t index,
                            int32_t arrayElement) override;
  ResourceId GetCurrentResource() override;

  Subresource GetSelectedSubresource() override;
  void SetSelectedSubresource(Subresource sub) override;
  rdcpair<int32_t, int32_t> GetPickedLocation() override;
  void GotoLocation(uint32_t x, uint32_t y) override;
  DebugOverlay GetTextureOverlay() override;
  void SetTextureOverlay(DebugOverlay overlay) override;

  TextureDisplay GetTextureDisplay() override { return m_TexDisplay; }
  bool IsZoomAutoFit() override;
  float GetZoomLevel() override;
  void SetZoomLevel(bool autofit, float zoom) override;

  rdcpair<float, float> GetHistogramRange() override;
  void SetHistogramRange(float blackpoint, float whitepoint) override;

  uint32_t GetChannelVisibilityBits() override;
  rdcfixedarray<bool, 4> GetChannelVisibility() override;
  void SetChannelVisibility(bool red, bool green, bool blue, bool alpha) override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private slots:
  // automatic slots
  void on_renderHScroll_valueChanged(int position);
  void on_renderVScroll_valueChanged(int position);

  void on_fitToWindow_toggled(bool checked);
  void on_zoomExactSize_clicked();
  void on_zoomOption_currentIndexChanged(int index);

  void on_mipLevel_currentIndexChanged(int index);
  void on_sliceFace_currentIndexChanged(int index);
  void on_overlay_currentIndexChanged(int index);

  void on_zoomRange_clicked();
  void on_autoFit_clicked();
  void on_autoFit_mouseClicked(QMouseEvent *e);
  void on_reset01_clicked();
  void on_visualiseRange_clicked();
  void on_backcolorPick_clicked();
  void on_checkerBack_clicked();

  void on_locationGoto_clicked();
  void on_viewTexBuffer_clicked();
  void on_resourceDetails_clicked();
  void on_texListShow_clicked();
  void on_saveTex_clicked();
  void on_debugPixelContext_clicked();
  void on_pixelHistory_clicked();
  void on_sbsToggle_clicked(bool checked);
  void on_jumpOtherEye_clicked();
  void updateSBSCompare();

  void on_customCreate_clicked();
  void on_customEdit_clicked();
  void on_customDelete_clicked();

  void on_cancelTextureListFilter_clicked();
  void on_textureListFilter_editTextChanged(const QString &text);
  void on_textureListFilter_currentIndexChanged(int index);
  void on_colSelect_clicked();
  void texture_itemActivated(RDTreeWidgetItem *item, int column);

  // manual slots
  void render_mouseClick(QMouseEvent *e);
  void render_mouseMove(QMouseEvent *e);
  void render_mouseWheel(QWheelEvent *e);
  void render_resize(QResizeEvent *e);
  void render_keyPress(QKeyEvent *e);

  void textureTab_Menu(const QPoint &pos);
  void textureTab_Changed(int index);
  void textureTab_Closing(int index);

  void thumb_clicked(QMouseEvent *);
  void thumb_doubleClicked(QMouseEvent *);
  void texContextItem_triggered();

  void zoomOption_returnPressed();

  void range_rangeUpdated();
  void rangePoint_textChanged(QString text);
  void rangePoint_leave();
  void rangePoint_keyPress(QKeyEvent *e);

  void customShaderModified(const QString &path);

  void channelsWidget_mouseClicked(QMouseEvent *event);
  void channelsWidget_toggled(bool checked) { UI_UpdateChannels(); }
  void channelsWidget_selected(int index) { UI_UpdateChannels(); }
protected:
  void enterEvent(QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void RT_FetchCurrentPixel(IReplayController *r, uint32_t x, uint32_t y, PixelValue &pickValue,
                            PixelValue &realValue);
  void RT_PickPixelsAndUpdate(IReplayController *);
  void RT_PickHoverAndUpdate(IReplayController *);
  void RT_UpdateAndDisplay(IReplayController *);
  void RT_UpdateVisualRange(IReplayController *);

  void UI_UpdateStatusText();
  void UI_UpdateTextureDetails();
  void UI_OnTextureSelectionChanged(bool newAction);

  void UI_SetHistogramRange(const TextureDescription *tex, CompType typeCast);

  void UI_UpdateChannels();

  void UI_UpdatePickedCrosshair();
  float SBSDynResHalfWidth();
  bool detectSBSFrame() const;
  void on_sbsSettings_clicked();

  void UI_UpdateSBSHeatmapAvailability();
  void enableSBSHeatmapFromOverlay();
  void disableSBSHeatmap();
  void rebuildSBSHeatmapShader();
  QString generateSBSHeatmapShader(const VRFrameBufferMatrices &mats, float dynResHalfW,
                                   float renderedW, float renderedH);

  // Resolves a single ViewProj/ViewProjInverse matrix set to use for reprojection: manual
  // override if enabled, else the first detected cbuffer candidate that passes verification
  // (or the specifically configured one). Must be called from the replay thread. Shared by
  // on_jumpOtherEye_clicked/updateSBSCompare/rebuildSBSHeatmapShader so the selection logic
  // only lives in one place.
  static bool ResolveSBSMatrices(IReplayController *r, bool useManualMats,
                                 const VRFrameBufferMatrices &manualMats,
                                 const rdcarray<StereoMatrixConfig> &matCandidates,
                                 int sbsCbufferIndex, VRFrameBufferMatrices &mats);

  void HighlightUsage();

  void SelectPreview(ResourcePreview *prev);

  void ShowPixelHistory(bool failedDebug);

  void SetupTextureTabs();
  void RemoveTextureTabs(int firstIndex);

  void Reset();

  void refreshTextureList();
  void refreshTextureList(FilterType filterType, const QString &filterStr);

  ResourcePreview *UI_CreateThumbnail(ThumbnailStrip *strip);
  void UI_CreateThumbnails();
  void InitResourcePreview(ResourcePreview *prev, Descriptor res, bool force, Following &follow,
                           const QString &bindName, const QString &slotName);

  void InitStageResourcePreviews(ShaderStage stage, const rdcarray<ShaderResource> &shaderInterface,
                                 const rdcarray<UsedDescriptor> &descriptors, ThumbnailStrip *prevs,
                                 int &prevIndex, bool copy, bool rw);

  void UI_PreviewResized(ResourcePreview *prev);

  void AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end, ResourceUsage usage);
  void OpenResourceContextMenu(ResourceId id, bool input, const rdcarray<EventUsage> &usage);

  void AutoFitRange();
  void rangePoint_Update();

  void updateBackgroundColors();

  bool currentTextureIsLocked() { return m_LockedId != ResourceId(); }
  void setFitToWindow(bool checked);

  void setCurrentZoomValue(float zoom);

  bool ScrollUpdateScrollbars = true;

  float CurMaxScrollX();
  float CurMaxScrollY();

  float GetFitScale();

  int realRenderWidth() const;
  int realRenderHeight() const;

  QPoint getScrollPosition();
  void setScrollPosition(const QPoint &pos);

  TextureDescription *GetCurrentTexture();
  void UI_UpdateCachedTexture();

  void ShowGotoPopup();

  // Formats the src/dst/delta HTML for the SBS eye-compare label.
  // tex is used to detect depth-stencil formats; may be NULL (falls back to float4).
  QString formatSBSCompareLabel(const PixelValue &srcVal, const PixelValue &dstVal,
                                uint32_t eyeIndex, CompType typeCast, const TextureDescription *tex);

  // Returns true when the currently-displayed texture is one of the output render targets
  // (colour or depth) bound at the current event. Used to gate viewport-based dynres detection:
  // the current viewport only reflects the rendering scale of the selected texture when it is
  // an output of the current draw; for input textures the viewport is irrelevant.
  bool isCurrentOutputTexture();
  void computeSBSDynResScale(uint32_t texW, uint32_t texH, float &scaleX, float &scaleY);
  const rdcarray<StereoMatrixConfig> &getCachedStereoMatrices() const;

  bool ShouldFlipForGL();
  uint32_t MipCoordFromBase(int coord, const uint32_t dim);
  uint32_t BaseCoordFromMip(int coord, const uint32_t dim);

  void UI_UpdateFittedScale();
  void UI_SetScale(float s);
  void UI_SetScale(float s, int x, int y);
  void UI_CalcScrollbars();

  QPoint m_DragStartScroll;
  QPoint m_DragStartPos;

  QPoint m_CurHoverPixel;
  QPoint m_PickedPoint;

  QSizeF m_PrevSize;

  PixelValue m_CurRealValue = {};
  PixelValue m_CurPixelValue = {};
  PixelValue m_CurHoverValue = {};

  QColor backCol;

  int m_HighWaterStatusLength = 0;
  int m_PrevFirstArraySlice = -1;
  int m_PrevHighestMip = -1;

  bool m_Visualise = false;
  bool m_NoRangePaint = false;
  bool m_RangePoint_Dirty = false;

  ResourceId m_LockedId;
  QMap<ResourceId, QWidget *> m_LockedTabs;
  int m_ResourceCacheID = -1;

  TextureGoto *m_Goto;

  Ui::TextureViewer *ui;
  ICaptureContext &m_Ctx;
  IReplayOutput *m_Output = NULL;

  TextureSave m_SaveConfig;

  bool m_NeedCustomReload = false;

  TextureDescription *m_CachedTexture;
  Following m_Following;
  QMap<ResourceId, TexSettings> m_TextureSettings;

  friend struct Following;

  rdcarray<UsedDescriptor> m_ReadOnlyResources[NumShaderStages];
  rdcarray<UsedDescriptor> m_ReadWriteResources[NumShaderStages];

  struct DescriptorThumbUpdate
  {
    DescriptorAccess access;
    ResourcePreview *preview;
    QString slotName;
  };

  rdcarray<DescriptorThumbUpdate> m_DescriptorThumbUpdates;

  QTime m_CustomShaderTimer;
  int m_CustomShaderWriteTime = 0;

  QFileSystemWatcher *m_Watcher = NULL;
  QStringList m_CustomShadersBusy;
  QMap<QString, ResourceId> m_CustomShaders;
  QMap<QString, IShaderViewer *> m_CustomShaderEditor;

  bool canCompileCustomShader(ShaderEncoding encoding);
  void reloadCustomShaders(const QString &filter);
  QList<QDir> getShaderDirectories() const;
  QString getShaderPath(const QString &filename) const;

  TextureDisplay m_TexDisplay;

  SBSMapper m_SBSMapper;

  // Four 1px solid border-line widgets forming a box around the picked pixel.
  // Using 4 thin widgets instead of one with a transparent interior avoids
  // the backing-store compositing issue where "transparent" shows the parent's
  // grey background instead of the OpenGL content below.
  QWidget *m_PickedCrosshair[4] = {};
  // Tracks whether the crosshair is currently using the dark (contrast) style to avoid
  // redundant setStyleSheet calls.
  bool m_CrosshairDark = false;
  // Last on-screen rect occupied by the crosshair. The render widget paints on-screen
  // (WA_PaintOnScreen) with no backing store, so when the marker widgets move the region
  // they vacate is not repainted and leaves ghost trails. We force a render-surface repaint
  // whenever this rect changes (move/appear/disappear) so the stale pixels are cleared.
  QRect m_LastCrosshairRect;

  // Pixel value comparison label populated after "Other Eye" jump.
  QLabel *m_SBSEyeCompare = NULL;

  // When false, matrix reprojection is skipped and the simple mirror fallback is always used.
  bool m_SBSMatrixReprojEnabled = true;

  // Which detected cbuffer candidate to use for matrix reprojection (-1 = auto, 0+ = specific index).
  int m_SBSCbufferIndex = -1;

  // Epsilon for eye-comparison delta coloring (neutral band around zero).
  double m_SBSDeltaEpsilon = 0.001;

  // Dynamic resolution override for SBS reprojection (0 = auto-detect).
  // Stores the full SBS rendered width; Y scale is derived proportionally (scaleY = scaleX).
  int m_SBSDynResW = 0;

  // When true, SBS mode is auto-enabled/disabled based on frame heuristics.
  bool m_SBSAutoEnable = true;

  // Tracks the last pixel pick that triggered an auto compare update.
  QPoint m_SBSLastAutoComparePick = QPoint(-1, -1);

  // Per-event cache for detectAllStereoMatrices. The result only depends on the pixel-shader
  // reflection at the current event, so it is valid for the entire event and must be invalidated
  // in OnEventChanged.
  mutable uint32_t m_SBSMatrixCacheEventId = ~0u;
  mutable rdcarray<StereoMatrixConfig> m_SBSMatrixCache;

  // Manual matrix override members (used when m_SBSUseManualMatrices is true).
  bool m_SBSUseManualMatrices = false;
  float m_SBSManualVP[2][16];
  float m_SBSManualVPInv[2][16];
  float m_SBSManualCamPos[2][4];

  // Anchor for "Jump to Other Eye" round-tripping. Reprojection is lossy (nearest-pixel
  // rounding plus per-pixel depth quantization), so bouncing A -> B -> A by re-running the
  // reprojection at B would drift from A. Instead, when the current pick is exactly the last
  // jump's destination, jump back to the exact remembered source instead of recomputing.
  // Reset in OnEventChanged since it is only valid for the event it was captured at.
  QPoint m_SBSAnchorPoint = QPoint(-1, -1);
  QPoint m_SBSJumpDestPoint = QPoint(-1, -1);

  // Full-image mismatch heatmap: flags pixels where left/right eye reprojection disagrees.
  // Rendered by a generated custom shader directly into the displayed image (see
  // rebuildSBSHeatmapShader()) - a real GPU overlay, not a Qt widget, so it composites
  // correctly with ui->render's WA_PaintOnScreen surface. D3D11-only for now (see
  // UI_UpdateSBSHeatmapAvailability()).
  bool m_SBSHeatmapEnabled = false;

  // When true, each match is also reprojected back to the source eye; drift beyond
  // m_SBSRoundTripPixelThreshold is flagged as "unreliable" (occlusion boundary) rather than
  // "mismatched", instead of relying on a one-way diff alone.
  bool m_SBSHeatmapRoundTripEnabled = false;
  double m_SBSRoundTripPixelThreshold = 1.5;

  // Minimum colour difference (0-255, display-encoded domain) before a pixel is flagged as
  // mismatched; which channel(s) this is measured on is m_SBSHeatmapChannel. Default of 10 is a
  // rough "human noticeable on casual viewing" threshold: single-step dithering/quantization
  // noise between independently-rendered eyes is typically within a few levels, while real
  // content differences (wrong geometry, missing effects, lighting divergence) tend to show up
  // well above that. Deliberately separate from m_SBSDeltaEpsilon, which compares raw pixel
  // values (not display bytes) for the single-pixel eye-compare label.
  double m_SBSHeatmapThreshold = 10.0;

  // Which channel(s) the heatmap threshold above is measured on: 0 = max(R,G,B) (any channel
  // mismatching is flagged - the default, catches anything), 1/2/3 = R/G/B only (isolates a
  // channel-specific bug, e.g. a post effect or LUT applied to only one eye, which often shows
  // as a hue/chroma shift more than an overall brightness change), 4 = luma (perceptual
  // brightness difference - ignores pure colour/hue shifts to focus on geometry/lighting
  // divergence, the kind of thing that actually reads as "wrong" at a glance in a VR headset).
  int m_SBSHeatmapChannel = 0;

  // The currently-compiled heatmap shader (rebuilt by rebuildSBSHeatmapShader() whenever the
  // event, matrices, or any of the settings above change) and whatever custom shader the user
  // had selected via ui->customShader before the heatmap toggle overrode it, restored when the
  // heatmap is turned back off.
  ResourceId m_SBSHeatmapShaderId;
  ResourceId m_SBSPrevCustomShaderId;

  // Debounces settings-dialog slider/combo changes so dragging doesn't recompile the shader on
  // every intermediate tick - restarted on each change, fires rebuildSBSHeatmapShader() ~150ms
  // after the last one.
  QTimer *m_SBSHeatmapRebuildDebounce = NULL;

  // Index of the "SBS Heatmap" entry appended to ui->overlay - not a real DebugOverlay value,
  // special-cased in on_overlay_currentIndexChanged to drive the heatmap shader instead.
  int m_SBSHeatmapOverlayIndex = -1;
};
