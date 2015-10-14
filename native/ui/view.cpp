﻿#include <queue>
#include <algorithm>

#include "base/mutex.h"
#include "base/stringutil.h"
#include "base/timeutil.h"
#include "input/input_state.h"
#include "input/keycodes.h"
#include "gfx_es2/draw_buffer.h"
#include "gfx/texture.h"
#include "gfx/texture_atlas.h"
#include "util/text/utf8.h"
#include "ui/ui.h"
#include "ui/view.h"
#include "ui/ui_context.h"
#include "thin3d/thin3d.h"
#include "base/NativeApp.h"

namespace UI {

static View *focusedView;
static bool focusMovementEnabled;
bool focusForced;
static recursive_mutex mutex_;

const float ITEM_HEIGHT = 64.f;


struct DispatchQueueItem {
	Event *e;
	EventParams params;
};

std::deque<DispatchQueueItem> g_dispatchQueue;


void EventTriggered(Event *e, EventParams params) {
	lock_guard guard(mutex_);

	DispatchQueueItem item;
	item.e = e;
	item.params = params;
	g_dispatchQueue.push_front(item);
}

void DispatchEvents() {
	lock_guard guard(mutex_);

	while (!g_dispatchQueue.empty()) {
		DispatchQueueItem item = g_dispatchQueue.back();
		g_dispatchQueue.pop_back();
		if (item.e) {
			item.e->Dispatch(item.params);
		}
	}
}

void RemoveQueuedEvents(View *v) {
	for (size_t i = 0; i < g_dispatchQueue.size(); i++) {
		if (g_dispatchQueue[i].params.v == v)
			g_dispatchQueue.erase(g_dispatchQueue.begin() + i);
	}
}

View *GetFocusedView() {
	return focusedView;
}

void SetFocusedView(View *view, bool force) {
	if (focusedView) {
		focusedView->FocusChanged(FF_LOSTFOCUS);
	}
	focusedView = view;
	if (focusedView) {
		focusedView->FocusChanged(FF_GOTFOCUS);
		if (force) {
			focusForced = true;
		}
	}
}

void EnableFocusMovement(bool enable) {
	focusMovementEnabled = enable;
	if (!enable) {
		if (focusedView) {
			focusedView->FocusChanged(FF_LOSTFOCUS); 
		}
		focusedView = 0;
	}
}

bool IsFocusMovementEnabled() {
	return focusMovementEnabled;
}

void MeasureBySpec(Size sz, float contentWidth, MeasureSpec spec, float *measured) {
	*measured = sz;
	if (sz == WRAP_CONTENT) {
		if (spec.type == UNSPECIFIED || spec.type == AT_MOST)
			*measured = contentWidth;
		else if (spec.type == EXACTLY)
			*measured = spec.size;
	} else if (sz == FILL_PARENT)	{
		if (spec.type == UNSPECIFIED)
			*measured = contentWidth;  // We have no value to set
		else
			*measured = spec.size;
	} else if (spec.type == EXACTLY || (spec.type == AT_MOST && *measured > spec.size)) {
		*measured = spec.size;
	}
}

void Event::Add(std::function<EventReturn(EventParams&)> func) {
	HandlerRegistration reg;
	reg.func = func;
	handlers_.push_back(reg);
}

// Call this from input thread or whatever, it doesn't matter
void Event::Trigger(EventParams &e) {
	EventTriggered(this, e);
}

// Call this from UI thread
EventReturn Event::Dispatch(EventParams &e) {
	for (auto iter = handlers_.begin(); iter != handlers_.end(); ++iter) {
		if ((iter->func)(e) == UI::EVENT_DONE) {
			// Event is handled, stop looping immediately. This event might even have gotten deleted.
			return UI::EVENT_DONE;
		}
	}
	return UI::EVENT_SKIPPED;
}

View::~View() {
	if (HasFocus())
		SetFocusedView(0);
	RemoveQueuedEvents(this);
}

void View::Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) {
	float contentW = 0.0f, contentH = 0.0f;
	GetContentDimensions(dc, contentW, contentH);
	MeasureBySpec(layoutParams_->width, contentW, horiz, &measuredWidth_);
	MeasureBySpec(layoutParams_->height, contentH, vert, &measuredHeight_);
}

// Default values

void View::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	w = 10.0f;
	h = 10.0f;
}

Point View::GetFocusPosition(FocusDirection dir) {
	// The +2/-2 is some extra fudge factor to cover for views sitting right next to each other.
	// Distance zero yields strange results otherwise.
	switch (dir) {
	case FOCUS_LEFT: return Point(bounds_.x + 2, bounds_.centerY());
	case FOCUS_RIGHT: return Point(bounds_.x2() - 2, bounds_.centerY());
	case FOCUS_UP: return Point(bounds_.centerX(), bounds_.y + 2);
	case FOCUS_DOWN: return Point(bounds_.centerX(), bounds_.y2() - 2);

	default:
		return bounds_.Center();
	}
}

bool View::SetFocus() {
	if (IsFocusMovementEnabled()) {
		if (CanBeFocused()) {
			SetFocusedView(this);
			return true;
		}
	}
	return false;
}

void Clickable::Click() {
	UI::EventParams e;
	e.v = this;
	OnClick.Trigger(e);
};

void Clickable::FocusChanged(int focusFlags) {
	if (focusFlags & FF_LOSTFOCUS) {
		down_ = false;
		dragging_ = false;
	}
}

void Clickable::Touch(const TouchInput &input) {
	if (!IsEnabled()) {
		dragging_ = false;
		down_ = false;
		return;
	}

	if (input.flags & TOUCH_DOWN) {
		if (bounds_.Contains(input.x, input.y)) {
			if (IsFocusMovementEnabled())
				SetFocusedView(this);
			dragging_ = true;
			down_ = true;
		} else {
			down_ = false;
			dragging_ = false;
		}
	} else if (input.flags & TOUCH_MOVE) {
		if (dragging_)
			down_ = bounds_.Contains(input.x, input.y);
	}
	if (input.flags & TOUCH_UP) {
		if ((input.flags & TOUCH_CANCEL) == 0 && dragging_ && bounds_.Contains(input.x, input.y)) {
			Click();
		}
		down_ = false;
		downCountDown_ = 0;
		dragging_ = false;
	}
}

// TODO: O/X confirm preference for xperia play?

bool IsAcceptKeyCode(int keyCode) {
	if (confirmKeys.empty()) {
		return keyCode == NKCODE_SPACE || keyCode == NKCODE_ENTER || keyCode == NKCODE_Z || keyCode == NKCODE_BUTTON_A || keyCode == NKCODE_BUTTON_CROSS || keyCode == NKCODE_BUTTON_1;
	} else {
		return std::find(confirmKeys.begin(), confirmKeys.end(), (keycode_t)keyCode) != confirmKeys.end();
	}
}

bool IsEscapeKeyCode(int keyCode) {
	if (cancelKeys.empty()) {
		return keyCode == NKCODE_ESCAPE || keyCode == NKCODE_BACK || keyCode == NKCODE_BUTTON_CIRCLE || keyCode == NKCODE_BUTTON_B || keyCode == NKCODE_BUTTON_2;
	} else {
		return std::find(cancelKeys.begin(), cancelKeys.end(), (keycode_t)keyCode) != cancelKeys.end();
	}
}

bool IsTabLeftKeyCode(int keyCode) {
	if (tabLeftKeys.empty()) {
		return keyCode == NKCODE_BUTTON_L1;
	} else {
		return std::find(tabLeftKeys.begin(), tabLeftKeys.end(), (keycode_t)keyCode) != tabLeftKeys.end();
	}
}

bool IsTabRightKeyCode(int keyCode) {
	if (tabRightKeys.empty()) {
		return keyCode == NKCODE_BUTTON_R1;
	} else {
		return std::find(tabRightKeys.begin(), tabRightKeys.end(), (keycode_t)keyCode) != tabRightKeys.end();
	}
}

bool Clickable::Key(const KeyInput &key) {
	if (!HasFocus() && key.deviceId != DEVICE_ID_MOUSE) {
		down_ = false;
		return false;
	}
	// TODO: Replace most of Update with this.
	bool ret = false;
	if (key.flags & KEY_DOWN) {
		if (IsAcceptKeyCode(key.keyCode)) {
			down_ = true;
			ret = true;
		}
	}
	if (key.flags & KEY_UP) {
		if (IsAcceptKeyCode(key.keyCode)) {
			if (down_) {
				Click();
				down_ = false;
				ret = true;
			}
		} else if (IsEscapeKeyCode(key.keyCode)) {
			down_ = false;
		}
	}
	return ret;
}

void StickyChoice::Touch(const TouchInput &input) {
	dragging_ = false;
	if (!IsEnabled()) {
		down_ = false;
		return;
	}

	if (input.flags & TOUCH_DOWN) {
		if (bounds_.Contains(input.x, input.y)) {
			if (IsFocusMovementEnabled())
				SetFocusedView(this);
			down_ = true;
			Click();
		}
	}
}

bool StickyChoice::Key(const KeyInput &key) {
	if (!HasFocus()) {
		return false;
	}

	// TODO: Replace most of Update with this.
	if (key.flags & KEY_DOWN) {
		if (IsAcceptKeyCode(key.keyCode)) {
			down_ = true;
			Click();
			return true;
		}
	}
	return false;
}

void StickyChoice::FocusChanged(int focusFlags) {
	// Override Clickable's FocusChanged to do nothing.
}

Item::Item(LayoutParams *layoutParams) : InertView(layoutParams) {
	if (!layoutParams) {
		layoutParams_->width = FILL_PARENT;
		layoutParams_->height = ITEM_HEIGHT;
	}
}

void Item::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	w = 0.0f;
	h = 0.0f;
}

void ClickableItem::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	w = 0.0f;
	h = 0.0f;
}

ClickableItem::ClickableItem(LayoutParams *layoutParams) : Clickable(layoutParams) {
	if (!layoutParams) {
		if (layoutParams_->width == WRAP_CONTENT)
			layoutParams_->width = FILL_PARENT;
		layoutParams_->height = ITEM_HEIGHT;
	}
}

void ClickableItem::Draw(UIContext &dc) {
	Style style =	dc.theme->itemStyle;

	if (HasFocus()) {
		style = dc.theme->itemFocusedStyle;
	}
	if (down_) {
		style = dc.theme->itemDownStyle;
	}

	dc.FillRect(style.background, bounds_);
}

void Choice::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	if (atlasImage_ != -1) {
		const AtlasImage &img = dc.Draw()->GetAtlas()->images[atlasImage_];
		w = img.w;
		h = img.h;
	} else {
		dc.MeasureText(dc.theme->uiFont, text_.c_str(), &w, &h);
	}
	w += 24;
	h += 16;
}

void Choice::HighlightChanged(bool highlighted){
	highlighted_ = highlighted;
}

void Choice::Draw(UIContext &dc) {
	if (!IsSticky()) {
		ClickableItem::Draw(dc);
	} else {
		Style style =	dc.theme->itemStyle;
		if (highlighted_) {
			style = dc.theme->itemHighlightedStyle;
		}
		if (down_) {
			style = dc.theme->itemDownStyle;
		}
		if (HasFocus()) {
			style = dc.theme->itemFocusedStyle;
		}
		dc.FillRect(style.background, bounds_);
	}

	Style style = dc.theme->itemStyle;
	if (!IsEnabled()) {
		style = dc.theme->itemDisabledStyle;
	}

	if (atlasImage_ != -1) {
		dc.Draw()->DrawImage(atlasImage_, bounds_.centerX(), bounds_.centerY(), 1.0f, style.fgColor, ALIGN_CENTER);
	} else {
		int paddingX = 12;
		dc.SetFontStyle(dc.theme->uiFont);
		if (centered_) {
			dc.DrawText(text_.c_str(), bounds_.centerX(), bounds_.centerY(), style.fgColor, ALIGN_CENTER);
		} else {
			if (iconImage_ != -1) {
				dc.Draw()->DrawImage(iconImage_, bounds_.x2() - 32 - paddingX, bounds_.centerY(), 0.5f, style.fgColor, ALIGN_CENTER);
			}
			dc.DrawText(text_.c_str(), bounds_.x + paddingX, bounds_.centerY(), style.fgColor, ALIGN_VCENTER);
		}
	}

	if (selected_) {
		dc.Draw()->DrawImage(dc.theme->checkOn, bounds_.x2() - 40, bounds_.centerY(), 1.0f, style.fgColor, ALIGN_CENTER);
	}
}

void InfoItem::Draw(UIContext &dc) {
	Item::Draw(dc);
	if (HasFocus()) {
		UI::Style style = dc.theme->itemFocusedStyle;
		style.background.color &= 0x7fffffff;
		dc.FillRect(style.background, bounds_);
	}
	int paddingX = 12;

	dc.SetFontStyle(dc.theme->uiFont);
	dc.DrawText(text_.c_str(), bounds_.x + paddingX, bounds_.centerY(), 0xFFFFFFFF, ALIGN_VCENTER);
	dc.DrawText(rightText_.c_str(), bounds_.x2() - paddingX, bounds_.centerY(), 0xFFFFFFFF, ALIGN_VCENTER | ALIGN_RIGHT);
// 	dc.Draw()->DrawImageStretch(dc.theme->whiteImage, bounds_.x, bounds_.y, bounds_.x2(), bounds_.y + 2, dc.theme->itemDownStyle.bgColor);
}

ItemHeader::ItemHeader(const std::string &text, LayoutParams *layoutParams)
	: Item(layoutParams), text_(text) {
		layoutParams_->width = FILL_PARENT;
		layoutParams_->height = 40;
}

void ItemHeader::Draw(UIContext &dc) {
	dc.SetFontStyle(dc.theme->uiFontSmall);
	dc.DrawText(text_.c_str(), bounds_.x + 4, bounds_.centerY(), 0xFFFFFFFF, ALIGN_LEFT | ALIGN_VCENTER);
	dc.Draw()->DrawImageStretch(dc.theme->whiteImage, bounds_.x, bounds_.y2()-2, bounds_.x2(), bounds_.y2(), 0xFFFFFFFF);
}

void PopupHeader::Draw(UIContext &dc) {
	const float paddingHorizontal = 12;
	const float availableWidth = bounds_.w - paddingHorizontal * 2;

	float tw, th;
	dc.SetFontStyle(dc.theme->uiFont);
	dc.MeasureText(dc.GetFontStyle(), text_.c_str(), &tw, &th, 0);

	float sineWidth = std::max(0.0f, (tw - availableWidth)) / 2.0f;

	float tx = paddingHorizontal;
	if (availableWidth < tw) {
		float overageRatio = 1.5f * availableWidth * 1.0f / tw;
		tx -= (1.0f + sin(time_now_d() * overageRatio)) * sineWidth;
		Bounds tb = bounds_;
		tb.x = bounds_.x + paddingHorizontal;
		tb.w = bounds_.w - paddingHorizontal * 2;
		dc.PushScissor(tb);
	}

	dc.DrawText(text_.c_str(), bounds_.x + tx, bounds_.centerY(), dc.theme->popupTitle.fgColor, ALIGN_LEFT | ALIGN_VCENTER);
	dc.Draw()->DrawImageStretch(dc.theme->whiteImage, bounds_.x, bounds_.y2()-2, bounds_.x2(), bounds_.y2(), dc.theme->popupTitle.fgColor);

	if (availableWidth < tw) {
		dc.PopScissor();
	}
}

void CheckBox::Toggle(){
	if (toggle_)
		*toggle_ = !(*toggle_);
};

EventReturn CheckBox::OnClicked(EventParams &e) {
	Toggle();
	return EVENT_CONTINUE;  // It's safe to keep processing events.
}

void CheckBox::Draw(UIContext &dc) {
	ClickableItem::Draw(dc);
	int paddingX = 12;

	int image = *toggle_ ? dc.theme->checkOn : dc.theme->checkOff;

	Style style = dc.theme->itemStyle;
	if (!IsEnabled())
		style = dc.theme->itemDisabledStyle;

	dc.SetFontStyle(dc.theme->uiFont);
	dc.DrawText(text_.c_str(), bounds_.x + paddingX, bounds_.centerY(), style.fgColor, ALIGN_VCENTER);
	dc.Draw()->DrawImage(image, bounds_.x2() - paddingX, bounds_.centerY(), 1.0f, style.fgColor, ALIGN_RIGHT | ALIGN_VCENTER);
}

void Button::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	if (imageID_ != -1) {
		const AtlasImage *img = &dc.Draw()->GetAtlas()->images[imageID_];
		w = img->w;
		h = img->h;
	} else {
		dc.MeasureText(dc.theme->uiFont, text_.c_str(), &w, &h);
	}
	// Add some internal padding to not look totally ugly
	w += 16;
	h += 8;
}

void Button::Draw(UIContext &dc) {
	Style style = dc.theme->buttonStyle;

	if (HasFocus()) style = dc.theme->buttonFocusedStyle;
	if (down_) style = dc.theme->buttonDownStyle;
	if (!IsEnabled()) style = dc.theme->buttonDisabledStyle;

	// dc.Draw()->DrawImage4Grid(style.image, bounds_.x, bounds_.y, bounds_.x2(), bounds_.y2(), style.bgColor);
	dc.FillRect(style.background, bounds_);
	float tw, th;
	dc.MeasureText(dc.theme->uiFont, text_.c_str(), &tw, &th);
	if (tw > bounds_.w || imageID_ != -1) {
		dc.PushScissor(bounds_);
	}
	dc.SetFontStyle(dc.theme->uiFont);
	if (imageID_ != -1 && text_.empty()) {
		dc.Draw()->DrawImage(imageID_, bounds_.centerX(), bounds_.centerY(), 1.0f, 0xFFFFFFFF, ALIGN_CENTER);
	} else if (!text_.empty()) {
		dc.DrawText(text_.c_str(), bounds_.centerX(), bounds_.centerY(), style.fgColor, ALIGN_CENTER);
		if (imageID_ != -1) {
			const AtlasImage &img = dc.Draw()->GetAtlas()->images[imageID_];
			dc.Draw()->DrawImage(imageID_, bounds_.centerX() - tw / 2 - 5 - img.w/2, bounds_.centerY(), 1.0f, 0xFFFFFFFF, ALIGN_CENTER);
		}
	}
	if (tw > bounds_.w || imageID_ != -1) {
		dc.PopScissor();
	}
}

void ImageView::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	const AtlasImage &img = dc.Draw()->GetAtlas()->images[atlasImage_];
	// TODO: involve sizemode
	w = img.w;
	h = img.h;
}

void ImageView::Draw(UIContext &dc) {
	const AtlasImage &img = dc.Draw()->GetAtlas()->images[atlasImage_];
	// TODO: involve sizemode
	float scale = bounds_.w / img.w;
	dc.Draw()->DrawImage(atlasImage_, bounds_.x, bounds_.y, scale, 0xFFFFFFFF, ALIGN_TOPLEFT);
}

void TextureView::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	// TODO: involve sizemode
	if (texture_) {
		w = (float)texture_->Width();
		h = (float)texture_->Height();
	} else {
		w = 16;
		h = 16;
	}
}

void TextureView::Draw(UIContext &dc) {
	// TODO: involve sizemode
	if (texture_) {
		dc.Flush();
		texture_->Bind(0);
		dc.Draw()->Rect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, color_);
		dc.Flush();
		dc.RebindTexture();
	}
}

void Thin3DTextureView::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	// TODO: involve sizemode
	if (texture_) {
		w = (float)texture_->Width();
		h = (float)texture_->Height();
	} else {
		w = 16;
		h = 16;
	}
}

void Thin3DTextureView::Draw(UIContext &dc) {
	// TODO: involve sizemode
	if (texture_) {
		dc.Flush();
		dc.GetThin3DContext()->SetTexture(0, texture_);
		dc.Draw()->Rect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, color_);
		dc.Flush();
		dc.RebindTexture();
	}
}

void TextView::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	// MeasureText doesn't seem to work with line breaks, so do something more sophisticated.
	std::vector<std::string> lines;
	SplitString(text_, '\n', lines);
	float total_w = 0.f;
	float total_h = 0.f;
	for (size_t i = 0; i < lines.size(); i++) {
		float temp_w, temp_h;
		dc.MeasureText(small_ ? dc.theme->uiFontSmall : dc.theme->uiFont, lines[i].c_str(), &temp_w, &temp_h);
		if (temp_w > total_w)
			total_w = temp_w;
		total_h += temp_h;
	}
	w = total_w;
	h = total_h;
}

void TextView::Draw(UIContext &dc) {
	float w, h;
	GetContentDimensions(dc, w, h);
	bool clip = false;
	if (w > bounds_.w || h > bounds_.h)
		clip = true;
	if (clip) {
		Bounds clipRect = bounds_.Expand(10);  // TODO: Remove this hackery
		dc.Flush();
		dc.PushScissor(clipRect);
	}
	// In case it's been made focusable.
	if (HasFocus()) {
		UI::Style style = dc.theme->itemFocusedStyle;
		style.background.color &= 0x7fffffff;
		dc.FillRect(style.background, bounds_);
	}
	dc.SetFontStyle(small_ ? dc.theme->uiFontSmall : dc.theme->uiFont);
	if (shadow_) {
		uint32_t shadowColor = 0x80000000;
		dc.DrawTextRect(text_.c_str(), bounds_, shadowColor, textAlign_);
	}
	dc.DrawTextRect(text_.c_str(), bounds_, textColor_, textAlign_);
	if (clip) {
		dc.PopScissor();
	}
}

TextEdit::TextEdit(const std::string &text, const std::string &placeholderText, LayoutParams *layoutParams)
  : View(layoutParams), text_(text), undo_(text), placeholderText_(placeholderText), maxLen_(255), ctrlDown_(false) {
	caret_ = (int)text_.size();
}

void TextEdit::Draw(UIContext &dc) {
	dc.PushScissor(bounds_);
	dc.SetFontStyle(dc.theme->uiFont);
	dc.FillRect(HasFocus() ? UI::Drawable(0x80000000) : UI::Drawable(0x30000000), bounds_);

	float textX = bounds_.x;
	float w, h;

	Bounds textBounds = bounds_;
	textBounds.x = textX;

	if (text_.empty()) {
		if (placeholderText_.size()) {
			dc.DrawTextRect(placeholderText_.c_str(), bounds_, 0x50FFFFFF, ALIGN_CENTER);
		}
	} else {
		dc.DrawTextRect(text_.c_str(), textBounds, 0xFFFFFFFF, ALIGN_VCENTER | ALIGN_LEFT);
	}

	if (HasFocus()) {
		// Hack to find the caret position. Might want to find a better way...
		dc.MeasureTextCount(dc.theme->uiFont, text_.c_str(), caret_, &w, &h, ALIGN_VCENTER | ALIGN_LEFT);
		float caretX = w;
		caretX += textX;

		if (caretX > bounds_.w) {
			// Scroll text to the left if the caret won't fit. Not ideal but looks better than not scrolling it.
			textX -= caretX - bounds_.w;
		}
		dc.FillRect(UI::Drawable(0xFFFFFFFF), Bounds(caretX - 1, bounds_.y + 2, 3, bounds_.h - 4));
	}
	dc.PopScissor();
}

void TextEdit::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	dc.MeasureText(dc.theme->uiFont, text_.size() ? text_.c_str() : "Wj", &w, &h);
	w += 2;
	h += 2;
}

// Handles both windows and unix line endings.
static std::string FirstLine(const std::string &text) {
	size_t pos = text.find("\r\n");
	if (pos != std::string::npos) {
		return text.substr(0, pos);
	}
	pos = text.find('\n');
	if (pos != std::string::npos) {
		return text.substr(0, pos);
	}
	return text;
}

void TextEdit::Touch(const TouchInput &touch) {
	if (touch.flags & TOUCH_DOWN) {
		if (bounds_.Contains(touch.x, touch.y)) {
			SetFocusedView(this, true);
		}
	}
}

bool TextEdit::Key(const KeyInput &input) {
	if (!HasFocus())
		return false;
	bool textChanged = false;
	// Process navigation keys. These aren't chars.
	if (input.flags & KEY_DOWN) {
		switch (input.keyCode) {
		case NKCODE_CTRL_LEFT:
		case NKCODE_CTRL_RIGHT:
			ctrlDown_ = true;
			break;
		case NKCODE_DPAD_LEFT:  // ASCII left arrow
			u8_dec(text_.c_str(), &caret_);
			break;
		case NKCODE_DPAD_RIGHT: // ASCII right arrow
			u8_inc(text_.c_str(), &caret_);
			break;
		case NKCODE_MOVE_HOME:
		case NKCODE_PAGE_UP:
			caret_ = 0;
			break;
		case NKCODE_MOVE_END:
		case NKCODE_PAGE_DOWN:
			caret_ = (int)text_.size();
			break;
		case NKCODE_FORWARD_DEL:
			if (caret_ < (int)text_.size()) {
				int endCaret = caret_;
				u8_inc(text_.c_str(), &endCaret);
				undo_ = text_;
				text_.erase(text_.begin() + caret_, text_.begin() + endCaret);
				textChanged = true;
			}
			break;
		case NKCODE_DEL:
			if (caret_ > 0) {
				int begCaret = caret_;
				u8_dec(text_.c_str(), &begCaret);
				undo_ = text_;
				text_.erase(text_.begin() + begCaret, text_.begin() + caret_);
				caret_--;
				textChanged = true;
			}
			break;
		case NKCODE_ENTER:
			{
				EventParams e;
				e.s = text_;
				OnEnter.Trigger(e);
				break;
			}
		case NKCODE_BACK:
		case NKCODE_ESCAPE:
			return false;
		}

		if (ctrlDown_) {
			switch (input.keyCode) {
			case NKCODE_C:
				// Just copy the entire text contents, until we get selection support.
				System_SendMessage("setclipboardtext", text_.c_str());
				break;
			case NKCODE_V:
				{
					std::string clipText = System_GetProperty(SYSPROP_CLIPBOARD_TEXT);
					clipText = FirstLine(clipText);
					if (clipText.size()) {
						// Until we get selection, replace the whole text
						undo_ = text_;
						text_.clear();
						caret_ = 0;

						size_t maxPaste = maxLen_ - text_.size();
						if (clipText.size() > maxPaste) {
							int end = 0;
							while ((size_t)end < maxPaste) {
								u8_inc(clipText.c_str(), &end);
							}
							if (end > 0) {
								u8_dec(clipText.c_str(), &end);
							}
							clipText = clipText.substr(0, end);
						}
						InsertAtCaret(clipText.c_str());
						textChanged = true;
					}
				}
				break;
			case NKCODE_Z:
				text_ = undo_;
				break;
			}
		}

		if (caret_ < 0) {
			caret_ = 0;
		}
		if (caret_ > (int)text_.size()) {
			caret_ = (int)text_.size();
		}
	}

	if (input.flags & KEY_UP) {
		switch (input.keyCode) {
		case NKCODE_CTRL_LEFT:
		case NKCODE_CTRL_RIGHT:
			ctrlDown_ = false;
			break;
		}
	}

	// Process chars.
	if (input.flags & KEY_CHAR) {
		int unichar = input.keyCode;
		if (unichar >= 0x20 && !ctrlDown_) {  // Ignore control characters.
			// Insert it! (todo: do it with a string insert)
			char buf[8];
			buf[u8_wc_toutf8(buf, unichar)] = '\0';
			if (strlen(buf) + text_.size() < maxLen_) {
				undo_ = text_;
				InsertAtCaret(buf);
				textChanged = true;
			}
		}
	}

	if (textChanged) {
		UI::EventParams e;
		e.v = this;
		OnTextChange.Trigger(e);
	}
	return true;
}

void TextEdit::InsertAtCaret(const char *text) {
	size_t len = strlen(text);
	for (size_t i = 0; i < len; i++) {
		text_.insert(text_.begin() + caret_, text[i]);
		caret_++;
	}
}

void ProgressBar::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	dc.MeasureText(dc.theme->uiFont, "  100%  ", &w, &h);
}

void ProgressBar::Draw(UIContext &dc) {
	char temp[32];
	sprintf(temp, "%i%%", (int)(progress_ * 100.0f));
	dc.Draw()->DrawImageStretch(dc.theme->whiteImage, bounds_.x, bounds_.y, bounds_.x + bounds_.w * progress_, bounds_.y2(), 0xc0c0c0c0);
	dc.SetFontStyle(dc.theme->uiFont);
	dc.DrawTextRect(temp, bounds_, 0xFFFFFFFF, ALIGN_CENTER);
}

void TriggerButton::Touch(const TouchInput &input) {
	if (input.flags & TOUCH_DOWN) {
		if (bounds_.Contains(input.x, input.y)) {
			down_ |= 1 << input.id;
		}
	}
	if (input.flags & TOUCH_MOVE) {
		if (bounds_.Contains(input.x, input.y))
			down_ |= 1 << input.id;
		else
			down_ &= ~(1 << input.id);
	}

	if (input.flags & TOUCH_UP) {
		down_ &= ~(1 << input.id);
	}

	if (down_ != 0) {
		*bitField_ |= bit_;
	} else {
		*bitField_ &= ~bit_;
	}
}

void TriggerButton::Draw(UIContext &dc) {
	dc.Draw()->DrawImage(imageBackground_, bounds_.centerX(), bounds_.centerY(), 1.0f, 0xFFFFFFFF, ALIGN_CENTER);
	dc.Draw()->DrawImage(imageForeground_, bounds_.centerX(), bounds_.centerY(), 1.0f, 0xFFFFFFFF, ALIGN_CENTER);
}

void TriggerButton::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	const AtlasImage &image = dc.Draw()->GetAtlas()->images[imageBackground_];
	w = image.w;
	h = image.h;
}

bool Slider::Key(const KeyInput &input) {
	if (HasFocus() && (input.flags & KEY_DOWN)) {
		switch (input.keyCode) {
		case NKCODE_DPAD_LEFT:
		case NKCODE_MINUS:
		case NKCODE_NUMPAD_SUBTRACT:
			*value_ -= step_;
			break;
		case NKCODE_DPAD_RIGHT:
		case NKCODE_PLUS:
		case NKCODE_NUMPAD_ADD:
			*value_ += step_;
			break;
		case NKCODE_PAGE_UP:
			*value_ -= step_ * 10;
			break;
		case NKCODE_PAGE_DOWN:
			*value_ += step_ * 10;
			break;
		case NKCODE_MOVE_HOME:
			*value_ = minValue_;
			break;
		case NKCODE_MOVE_END:
			*value_ = maxValue_;
			break;
		default:
			return false;
		}
		Clamp();
		return true;
	} else {
		return false;
	}
}

void Slider::Touch(const TouchInput &input) {
	Clickable::Touch(input);
	if (dragging_ || bounds_.Contains(input.x, input.y)) {
		float relativeX = (input.x - (bounds_.x + paddingLeft_)) / (bounds_.w - paddingLeft_ - paddingRight_);
		*value_ = floorf(relativeX * (maxValue_ - minValue_) + minValue_ + 0.5f);
		Clamp();
	}
}

void Slider::Clamp() {
	if (*value_ < minValue_) *value_ = minValue_;
	else if (*value_ > maxValue_) *value_ = maxValue_;

	// Clamp the value to be a multiple of the nearest step (e.g. if step == 5, value == 293, it'll round down to 290).
	*value_ = *value_ - fmodf(*value_, step_);
}

void Slider::Draw(UIContext &dc) {
	bool focus = HasFocus();
	uint32_t linecolor = dc.theme->popupTitle.fgColor;
	uint32_t knobcolor = (down_ || focus) ? dc.theme->popupTitle.fgColor : 0xFFFFFFFF;
	float knobX = ((float)(*value_) - minValue_) / (maxValue_ - minValue_) * (bounds_.w - paddingLeft_ - paddingRight_) + (bounds_.x + paddingLeft_);
	dc.FillRect(Drawable(linecolor), Bounds(bounds_.x + paddingLeft_, bounds_.centerY() - 2, knobX - (bounds_.x + paddingLeft_), 4));
	dc.FillRect(Drawable(0xFF808080), Bounds(knobX, bounds_.centerY() - 2, (bounds_.x + bounds_.w - paddingRight_ - knobX), 4));
	dc.Draw()->DrawImage(dc.theme->sliderKnob, knobX, bounds_.centerY(), 1.0f, knobcolor, ALIGN_CENTER);
	char temp[64];
	if (showPercent_)
		sprintf(temp, "%i%%", *value_);
	else
		sprintf(temp, "%i", *value_);
	dc.SetFontStyle(dc.theme->uiFont);
	dc.DrawText(temp, bounds_.x2() - 22, bounds_.centerY(), 0xFFFFFFFF, ALIGN_CENTER);
}

void Slider::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	// TODO
	w = 100;
	h = 50;
}

bool SliderFloat::Key(const KeyInput &input) {
	if (HasFocus() && (input.flags & KEY_DOWN)) {
		switch (input.keyCode) {
		case NKCODE_DPAD_LEFT:
		case NKCODE_MINUS:
		case NKCODE_NUMPAD_SUBTRACT:
			*value_ -= (maxValue_ - minValue_) / 20.0f;
			break;
		case NKCODE_DPAD_RIGHT:
		case NKCODE_PLUS:
		case NKCODE_NUMPAD_ADD:
			*value_ += (maxValue_ - minValue_) / 30.0f;
			break;
		case NKCODE_PAGE_UP:
			*value_ -= (maxValue_ - minValue_) / 5.0f;
			break;
		case NKCODE_PAGE_DOWN:
			*value_ += (maxValue_ - minValue_) / 5.0f;
			break;
		case NKCODE_MOVE_HOME:
			*value_ = minValue_;
			break;
		case NKCODE_MOVE_END:
			*value_ = maxValue_;
			break;
		default:
			return true;
		}
		Clamp();
		return true;
	} else {
		return false;
	}
}

void SliderFloat::Touch(const TouchInput &input) {
	if (dragging_ || bounds_.Contains(input.x, input.y)) {
		float relativeX = (input.x - (bounds_.x + paddingLeft_)) / (bounds_.w - paddingLeft_ - paddingRight_);
		*value_ = (relativeX * (maxValue_ - minValue_) + minValue_);
		Clamp();
	}
}

void SliderFloat::Clamp() {
	if (*value_ < minValue_)
		*value_ = minValue_;
	else if (*value_ > maxValue_)
		*value_ = maxValue_;
}

void SliderFloat::Draw(UIContext &dc) {
	bool focus = HasFocus();
	uint32_t linecolor = dc.theme->popupTitle.fgColor;
	uint32_t knobcolor = (down_ || focus) ? dc.theme->popupTitle.fgColor : 0xFFFFFFFF;

	float knobX = (*value_ - minValue_) / (maxValue_ - minValue_) * (bounds_.w - paddingLeft_ - paddingRight_) + (bounds_.x + paddingLeft_);
	dc.FillRect(Drawable(linecolor), Bounds(bounds_.x + paddingLeft_, bounds_.centerY() - 2, knobX - (bounds_.x + paddingLeft_), 4));
	dc.FillRect(Drawable(0xFF808080), Bounds(knobX, bounds_.centerY() - 2, (bounds_.x + bounds_.w - paddingRight_ - knobX), 4));
	dc.Draw()->DrawImage(dc.theme->sliderKnob, knobX, bounds_.centerY(), 1.0f, knobcolor, ALIGN_CENTER);
	char temp[64];
	sprintf(temp, "%0.2f", *value_);
	dc.SetFontStyle(dc.theme->uiFont);
	dc.DrawText(temp, bounds_.x2() - 22, bounds_.centerY(), 0xFFFFFFFF, ALIGN_CENTER);
}

void SliderFloat::GetContentDimensions(const UIContext &dc, float &w, float &h) const {
	// TODO
	w = 100;
	h = 50;
}

}  // namespace
