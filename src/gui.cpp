
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/choice.h>
#include <wx/colour.h>
#include <wx/pen.h>
#include <wx/brush.h>
#include <wx/gdicmn.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/filedlg.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

#include "model.h"
#include "fish.h"
#include "hydro.h"

wxPen infoBorderPen(wxColour(72, 72, 72));
wxBrush infoBgBrush(wxColour(255, 255, 255, 192), wxSOLID);

wxColour IMPOUNDMENT_COLOUR(64U, 255U, 128U);
wxColour DISTRIBUTARY_COLOUR(0U, 0U, 255U);
wxColour DISTRIBUTARY_EDGE_COLOUR(0U, 128U, 255U);
wxColour BLIND_CHANNEL_COLOUR(0U, 128U, 0U);
wxColour LOW_TIDE_TERRACE_COLOUR(64U, 128U, 128U);
wxColour NEARSHORE_COLOUR(128U, 0U, 128U);
wxColour BOAT_HARBOR_COLOUR(128U, 128U, 0U);

wxBrush samplingSiteHighlightBrush(wxColour(255, 192, 0, 128), wxSOLID);

wxPen upstreamHighlightPen(wxColour(255, 0, 0, 64), 2);
wxPen downstreamHighlightPen(wxColour(0, 0, 255, 64), 2);

wxColour reachableOnlyColor(0, 64, 255, 128);

wxColour *getHabitatColor(HabitatType t) {
    switch(t) {
    case HabitatType::Impoundment:
        return &IMPOUNDMENT_COLOUR;
    case HabitatType::Distributary:
        return &DISTRIBUTARY_COLOUR;
    case HabitatType::DistributaryEdge:
        return &DISTRIBUTARY_EDGE_COLOUR;
    case HabitatType::BlindChannel:
        return &BLIND_CHANNEL_COLOUR;
    case HabitatType::LowTideTerrace:
        return &LOW_TIDE_TERRACE_COLOUR;
    case HabitatType::Nearshore:
        return &NEARSHORE_COLOUR;
    case HabitatType::Harbor:
        return &BOAT_HARBOR_COLOUR;
    }
}

std::vector<HabitatType> legendHabitatTypes {
    HabitatType::Impoundment,
    HabitatType::Distributary,
    HabitatType::DistributaryEdge,
    HabitatType::BlindChannel,
    HabitatType::LowTideTerrace,
    HabitatType::Nearshore,
    HabitatType::Harbor
};

std::string getHabTypeName(HabitatType t) {
    switch(t) {
    case HabitatType::Impoundment:
        return "Impoundment";
    case HabitatType::Distributary:
        return "Distributary";
    case HabitatType::DistributaryEdge:
        return "Distributary edge";
    case HabitatType::BlindChannel:
        return "Blind channel";
    case HabitatType::LowTideTerrace:
        return "Low tide terrace";
    case HabitatType::Nearshore:
        return "Nearshore";
    case HabitatType::Harbor:
        return "Boat harbor";
    }
}

std::string getStatusName(FishStatus s) {
    switch(s) {
    case FishStatus::Alive:
        return "Alive";
    case FishStatus::DeadMortality:
        return "Dead (Mortality)";
    case FishStatus::DeadStranding:
        return "Dead (Stranding)";
    case FishStatus::DeadStarvation:
        return "Dead (Starvation)";
    case FishStatus::Exited:
        return "Exited";
    }
}

std::vector<std::pair<std::string, int>> MONTHS {
    {"January", 31},
    {"February", 28},
    {"March", 31},
    {"April", 30},
    {"May", 31},
    {"June", 30},
    {"July", 31},
    {"August", 31},
    {"September", 30},
    {"October", 31},
    {"November", 30},
    {"December", 31}
};

void getMonthAndDayOfMonth(int day, std::string &monthOut, int &dayOfMonthOut) {
    for (std::pair<std::string, int> monthAndLength : MONTHS) {
        if (day < monthAndLength.second) {
            monthOut = monthAndLength.first;
            dayOfMonthOut = day + 1;
            return;
        }
        day -= monthAndLength.second;
    }
}

std::string formatTimestep(Model &model, int timestep) {
    int globalTimestep = timestep + model.globalTimeIntercept;
    int day = globalTimestep / 24;
    std::string month;
    int dayOfMonth;
    getMonthAndDayOfMonth(day, month, dayOfMonth);
    int hour = globalTimestep % 24;
    std::string amPm = hour < 12 ? "am" : "pm";
    int minute = 0;
    std::string minuteStr = std::to_string(minute);
    std::ostringstream os;
    os << month << " " << dayOfMonth << ", " << ((hour + 11) % 12 + 1) << ":" << minuteStr;
    if (minuteStr.length() < 2) {
        os << '0';
    }
    os << amPm;
    return os.str();
}

wxColour getRangeColor(float rangeVal) {
    if (rangeVal == 0.0f) {
        return reachableOnlyColor;
    }
    unsigned char r = (unsigned char) (255.0f * std::min(1.0f, 2.0f-2.0f*rangeVal));
    unsigned char g = (unsigned char) (255.0f * std::min(1.0f, 2.0f*rangeVal));
    return wxColour(r, g, 0U, 128U);
}

std::vector<std::string> getLocInfo(Model &model, MapNode &node) {
    std::vector<std::string> result;
    std::ostringstream os;
    os << "Node ID: " << node.id + 1; result.push_back(os.str()); os.str("");
    os << "Pop. density: " << node.popDensity; result.push_back(os.str()); os.str("");
    os << "Habitat type: " << getHabTypeName(node.type); result.push_back(os.str()); os.str("");
    os << "Elevation: " << node.elev << "m"; result.push_back(os.str()); os.str("");
    os << "Area: " << node.area << "m2"; result.push_back(os.str()); os.str("");
    os << "Depth: " << model.hydroModel.getDepth(node) << "m"; result.push_back(os.str()); os.str("");
    os << "Temp: " << model.hydroModel.getTemp(node) << "C"; result.push_back(os.str()); os.str("");
    os << "Flow speed: " << model.hydroModel.getUnsignedFlowSpeedAt(node) << "m/s"; result.push_back(os.str()); os.str("");
    os << "Flow velocity (u, v): " << model.hydroModel.getCurrentU(node) << ", " << model.hydroModel.getCurrentV(node) << "m/s"; result.push_back(os.str()); os.str("");
    for (SamplingSite *site : model.samplingSites) {
        for (MapNode *point : site->points) {
            if (point->id == node.id) {
                os << "Part of sampling site " << site->siteName; result.push_back(os.str()); os.str("");
            }
        }
    }
    return result;
}

std::vector<std::string> getFishInfo(Model &model, Fish &fish) {
    std::vector<std::string> result;
    std::ostringstream os;
    os << "Fish ID: " << fish.id; result.push_back(os.str()); os.str("");
    os << "Status: " << getStatusName(fish.status); result.push_back(os.str()); os.str("");
    os << "Spawn time: " << formatTimestep(model, fish.spawnTime); result.push_back(os.str()); os.str("");
    os << "Fork length: " << fish.forkLength << "mm"; result.push_back(os.str()); os.str("");
    os << "Mass: " << fish.mass << "g"; result.push_back(os.str()); os.str("");
    os << "Pmax: " << fish.lastPmax << "p"; result.push_back(os.str()); os.str("");
    os << "Growth: " << fish.lastGrowth << "g"; result.push_back(os.str()); os.str("");
    os << "Mortality: " << fish.lastMortality; result.push_back(os.str()); os.str("");
    os << "Temp: " << fish.lastTemp; result.push_back(os.str()); os.str("");
    os << "Depth: " << fish.lastDepth; result.push_back(os.str()); os.str("");
    os << "Flow speed: " << fish.lastFlowSpeed_old; result.push_back(os.str()); os.str("");
    os << "Flow velocity (u, v): " << fish.lastFlowVelocity.u << ", " << fish.lastFlowVelocity.v; result.push_back(os.str()); os.str("");
    if (fish.taggedTime != -1) {
        os << "Tagged at timestep " << fish.taggedTime; result.push_back(os.str()); os.str("");
    }
    return result;
}

std::vector<std::string> getPopInfo(Model &model) {
    std::vector<std::string> result;
    std::ostringstream os;
    os << "Living pop.: " << model.livingIndividuals.size(); result.push_back(os.str()); os.str("");
    os << "Dead pop.: " << model.deadCount; result.push_back(os.str()); os.str("");
    os << "Exited pop.: " << model.exitedCount; result.push_back(os.str()); os.str("");
    return result;
}

std::vector<std::string> getTimeInfo(Model &model) {
    std::vector<std::string> result;
    std::ostringstream os;
    os << "Time: " << formatTimestep(model, model.time); result.push_back(os.str()); os.str("");
    os << "Timestep: " << model.time; result.push_back(os.str()); os.str("");
    return result;
}

std::vector<std::string> getLegendText() {
    std::vector<std::string> result;
    for (HabitatType t : legendHabitatTypes) {
        result.push_back(getHabTypeName(t));
    }
    return result;
}

std::string strJoin(std::string sep, std::vector<std::string> strs) {
    std::ostringstream os;
    bool first = true;
    for (std::string str : strs) {
        if (first) {
            first = false;
        } else {
            os << sep;
        }
        os << str;
    }
    return os.str();
}

void getTextExtentMultiline(wxDC &dc, std::vector<std::string> &lines, int &w, int &h) {
    w = 0;
    h = 0;
    for (std::string &line : lines) {
        wxSize s = dc.GetTextExtent(line);
        w = std::max(w, s.GetWidth());
        h += s.GetHeight();
    }
}

class FishGui : public wxApp {
public:
    virtual bool OnInit();
};

class MapView : public wxPanel {
public:
    MapView(wxWindow *parent, Model *model, wxChoice *fishSelector, wxButton *tagButton, int w, int h);
    Model *model;
    float minX;
    float minY;
    float maxX;
    float maxY;
    float mapW;
    float mapH;
    float mapCenterX;
    float mapCenterY;
    float viewZoom;
    wxBitmap *_buffer;
    wxBitmap *fishIcon;
    bool isGrabbed;
    wxPoint mouseGrabCoords;
    float preGrabCenterX;
    float preGrabCenterY;
    MapNode *selectedNode;
    std::unordered_set<MapNode *> mapSet;
    long selectedFishId;
    std::unordered_map<MapNode *, float> selectedFishRange;
    wxChoice *fishSelector;
    wxButton *tagButton;
    void OnSize(wxSizeEvent &evt);
    void OnPaint(wxPaintEvent &evt);
    void redraw(wxDC &drawContext, float centerX, float centerY, float viewW, float viewH);
    void grab(wxMouseEvent &evt);
    void release(wxMouseEvent &evt);
    void mouseMove(wxMouseEvent &evt);
    void selectNode(wxMouseEvent &evt);
    void updateDropdown();
    void selectFish(wxCommandEvent &evt);
    void updateSelectedFishRange();
    void tagFish(wxCommandEvent &event);
    void onModelUpdate();
    void onModelReset();

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MapView, wxPanel)
    EVT_SIZE(MapView::OnSize)
    EVT_PAINT(MapView::OnPaint)
    EVT_LEFT_DOWN(MapView::grab)
    EVT_MOTION(MapView::mouseMove)
    EVT_LEFT_UP(MapView::release)
    EVT_LEFT_DCLICK(MapView::selectNode)
wxEND_EVENT_TABLE()

MapView::MapView(wxWindow *parent, Model *model, wxChoice *fishSelector, wxButton *tagButton, int w, int h)
    : wxPanel(parent), model(model), _buffer(nullptr),
    isGrabbed(false), selectedNode(nullptr), selectedFishId(-1L), fishSelector(fishSelector), tagButton(tagButton)
{
    bool first = true;
    for (MapNode *n : model->map) {
        this->mapSet.insert(n);
        if (first) {
            this->minX = this->maxX = n->x;
            this->minY = this->maxY = n->y;
            first = false;
        } else {
            this->minX = std::min(this->minX, n->x);
            this->minY = std::min(this->minY, n->y);
            this->maxX = std::max(this->maxX, n->x);
            this->maxY = std::max(this->maxY, n->y);
        }
    }
    this->mapW = this->maxX - this->minX;
    this->mapH = this->maxY - this->minY;
    this->mapCenterX = this->minX + this->mapW / 2.0f;
    this->mapCenterY = this->minY + this->mapH / 2.0f;
    this->viewZoom = std::min(((float) w) / this->mapW, ((float) h) / this->mapH);
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);
    this->SetBackgroundColour(*wxWHITE);
    wxImage::AddHandler(new wxPNGHandler);
    this->fishIcon = new wxBitmap(wxImage("fish_icon.png", wxBITMAP_TYPE_PNG));
    this->fishSelector->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &MapView::selectFish, this);
    this->tagButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &MapView::tagFish, this);
    this->Show();
}

void MapView::OnSize(wxSizeEvent &evt) {
    evt.Skip();
    this->Refresh();
}

inline float zoom(float x, float mapCenter, float viewCenter, float zoomFactor) {
    return (x - mapCenter)*zoomFactor + viewCenter;
}

inline float unzoom(float px, float mapCenter, float viewCenter, float zoomFactor) {
    return (px - viewCenter)/zoomFactor + mapCenter;
}

void MapView::OnPaint(wxPaintEvent &evt) {
    int w;
    int h;
    this->GetClientSize(&w, &h);
    wxAutoBufferedPaintDC dc((wxWindow *) this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    if (this->_buffer == nullptr) {
        int fullW = (int) (this->viewZoom * this->mapW);
        int fullH = (int) (this->viewZoom * this->mapH);
        if (fullW > 2*w && fullH > 2*h) {
            this->redraw(dc, this->mapCenterX, this->mapCenterY, (float) w, (float) h);
        } else {
            this->_buffer = new wxBitmap(fullW, fullH);
            wxMemoryDC mdc(*this->_buffer);
            mdc.SetBackground(*wxWHITE_BRUSH);
            mdc.Clear();
            this->redraw(mdc, this->minX + this->mapW/2.0f, this->minY + this->mapH/2.0f, (float) fullW, (float) fullH);
        }
    }
    if (this->_buffer != nullptr) {
        dc.DrawBitmap(*this->_buffer,
            (int) zoom(this->minX, this->mapCenterX, ((float) w)/2.0f, this->viewZoom),
            (int) zoom(this->maxY, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom)
        );
    }
    if (this->selectedNode != nullptr) {
        dc.SetBrush(wxNullBrush);
        dc.SetPen(*wxGREEN_PEN);
        int sx = (int) zoom(this->selectedNode->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
        int sy = (int) zoom(this->selectedNode->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
        dc.DrawCircle(sx, sy, 6);
        /*
        if (this->selectedNode->crossChannelA != nullptr) {
            int ccAx0 = (int) zoom(this->selectedNode->crossChannelA->source->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int ccAy0 = (int) zoom(this->selectedNode->crossChannelA->source->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            int ccAx1 = (int) zoom(this->selectedNode->crossChannelA->target->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int ccAy1 = (int) zoom(this->selectedNode->crossChannelA->target->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            dc.DrawLine(ccAx0, ccAy0, ccAx1, ccAy1);
        }
        if (this->selectedNode->crossChannelB != nullptr) {
            dc.SetPen(*wxGREEN_PEN);
            int ccBx0 = (int) zoom(this->selectedNode->crossChannelA->source->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int ccBy0 = (int) zoom(this->selectedNode->crossChannelA->source->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            int ccBx1 = (int) zoom(this->selectedNode->crossChannelA->target->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int ccBy1 = (int) zoom(this->selectedNode->crossChannelA->target->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            dc.DrawLine(ccBx0, ccBy0, ccBx1, ccBy1);
        }
        */
        dc.SetPen(upstreamHighlightPen);
        dc.SetTextForeground(wxColour(128,128,128,128));
        for (Edge &e : this->selectedNode->edgesIn) {
            int x1 = (int) zoom(e.source->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int y1 = (int) zoom(e.source->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            std::string edgeLabel = std::to_string((int) e.length);
            wxSize s = dc.GetTextExtent(edgeLabel);
            if (abs(x1 - sx) > s.GetWidth() || abs(y1 - sy) > s.GetHeight()) {
                dc.DrawText(edgeLabel, (x1 + sx)/2 - s.GetWidth()/2, (y1 + sy)/2 - s.GetHeight()/2);
            }
            dc.DrawLine(sx, sy, x1, y1);
        }
        dc.SetPen(downstreamHighlightPen);
        for (Edge &e : this->selectedNode->edgesOut) {
            int x1 = (int) zoom(e.target->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int y1 = (int) zoom(e.target->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            std::string edgeLabel = std::to_string((int) e.length);
            wxSize s = dc.GetTextExtent(edgeLabel);
            if (abs(x1 - sx) > s.GetWidth() || abs(y1 - sy) > s.GetHeight()) {
                dc.DrawText(edgeLabel, (x1 + sx)/2 - s.GetWidth()/2, (y1 + sy)/2 - s.GetHeight()/2);
            }
            dc.DrawLine(sx, sy, x1, y1);
        }
        dc.SetTextForeground(*wxBLACK);
        dc.SetPen(wxNullPen);
        if (this->selectedFishId != -1) {
            for (auto it = this->selectedFishRange.begin(); it != this->selectedFishRange.end(); ++it) {
                MapNode *n = it->first;
                int x0 = (int) zoom(n->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
                int y0 = (int) zoom(n->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
                dc.SetBrush(wxBrush(getRangeColor(it->second)));
                dc.DrawCircle(x0, y0, 6);
            }
        }
        dc.SetPen(infoBorderPen);
        dc.SetBrush(infoBgBrush);
        std::vector<std::string> text = getLocInfo(*this->model, *this->selectedNode);
        int textW; int textH;
        getTextExtentMultiline(dc, text, textW, textH);
        dc.DrawRectangle(w - textW - 5 - 10, 5, textW + 10, textH + 10);
        dc.DrawText(strJoin("\n", text), w - textW - 10, 10);
        if (this->selectedFishId != -1) {
            Fish &selectedFish = this->model->individuals[this->selectedFishId];
            std::vector<std::string> text2 = getFishInfo(*this->model, selectedFish);
            int text2W; int text2H;
            getTextExtentMultiline(dc, text2, text2W, text2H);
            dc.DrawRectangle(w - text2W - 5 - 10, 5 + textH + 10 + 5, text2W + 10, text2H + 10);
            dc.DrawText(strJoin("\n", text2), w - text2W - 10, textH + 10 + 5 + 10);
        }
    }
    for (MapNode *n : this->model->map) {
        if (n->popDensity > 0.0f) {
            int x0 = (int) zoom(n->x, this->mapCenterX, ((float) w)/2.0f, this->viewZoom);
            int y0 = (int) zoom(n->y, this->mapCenterY, ((float) h)/2.0f, -this->viewZoom);
            dc.DrawBitmap(*this->fishIcon, x0-8, y0-8);
        }
    }
    dc.SetPen(infoBorderPen);
    dc.SetBrush(infoBgBrush);
    std::vector<std::string> popText = getPopInfo(*this->model);
    int popTextW; int popTextH;
    getTextExtentMultiline(dc, popText, popTextW, popTextH);
    dc.DrawRectangle(5, h - popTextH - 5 - 10, popTextW + 10, popTextH + 10);
    dc.DrawText(strJoin("\n", popText), 10, h - popTextH - 10);
    std::vector<std::string> timeText = getTimeInfo(*this->model);
    int timeTextW; int timeTextH;
    getTextExtentMultiline(dc, timeText, timeTextW, timeTextH);
    dc.DrawRectangle(5, 5, timeTextW + 10, timeTextH + 10);
    dc.DrawText(strJoin("\n", timeText), 10, 10);

    // draw legend

    dc.SetPen(infoBorderPen);
    dc.SetBrush(infoBgBrush);
    std::vector<std::string> legendText = getLegendText();
    int legendW; int legendH;
    getTextExtentMultiline(dc, legendText, legendW, legendH);
    dc.DrawRectangle(w - legendW - 5 - 10 - 20, h - legendH - 5 - 10, legendW + 10 + 20, legendH + 10);
    dc.DrawText(strJoin("\n", legendText), w - legendW - 10, h - legendH - 10);
    for (size_t i = 0; i < legendHabitatTypes.size(); ++i) {
        dc.SetPen(wxPen(*getHabitatColor(legendHabitatTypes[i])));
        int lineY = h - legendH + (int) (i * ((double) legendH)/(double) legendHabitatTypes.size());
        dc.DrawLine(w - legendW - 5 - 10 - 15, lineY, w - legendW - 5 - 10, lineY);
    }
}

void MapView::redraw(wxDC &dc, float centerX, float centerY, float viewW, float viewH) {
    for (MapNode *n : this->model->map) {
        int x0 = (int) zoom(n->x, centerX, viewW/2.0f, this->viewZoom);
        int y0 = (int) zoom(n->y, centerY, viewH/2.0f, -this->viewZoom);
        if (x0 < 0 || x0 > ((int) viewW) || y0 < 0 || y0 > ((int) viewH)) {
            continue;
        }
        for (Edge &e : n->edgesOut) {
            if (!this->mapSet.count(e.target)) {
                dc.SetBrush(*wxRED_BRUSH);
                dc.DrawCircle(x0, y0, 3);
                dc.SetBrush(wxNullBrush);
                continue;
            }
            if (n->type == HabitatType::DistributaryEdge && e.target->type == HabitatType::DistributaryEdge) {
                dc.SetPen(wxPen(*getHabitatColor(HabitatType::DistributaryEdge)));
            } else {
                dc.SetPen(wxPen(*getHabitatColor(n->type)));
            }
            int x1 = (int) zoom(e.target->x, centerX, viewW/2.0f, this->viewZoom);
            int y1 = (int) zoom(e.target->y, centerY, viewH/2.0f, -this->viewZoom);
            dc.DrawLine(x0, y0, x1, y1);
        }
        for (Edge &e : n->edgesIn) {
            if (!this->mapSet.count(e.source)) {
                dc.SetBrush(*wxRED_BRUSH);
                dc.DrawCircle(x0, y0, 3);
                dc.SetBrush(wxNullBrush);
                continue;
            }
            int x1 = (int) zoom(e.source->x, centerX, viewW/2.0f, this->viewZoom);
            int y1 = (int) zoom(e.source->y, centerY, viewH/2.0f, -this->viewZoom);
            if (x1 < 0 || x1 > ((int) viewW) || y1 < 0 || y1 > ((int) viewH)) {
                if (n->type == HabitatType::DistributaryEdge && e.source->type == HabitatType::DistributaryEdge) {
                    dc.SetPen(wxPen(*getHabitatColor(HabitatType::DistributaryEdge)));
                } else {
                    dc.SetPen(wxPen(*getHabitatColor(e.source->type)));
                }
                dc.DrawLine(x0, y0, x1, y1);
            }
        }
    }
    dc.SetPen(wxNullPen);
    dc.SetBrush(samplingSiteHighlightBrush);
    for (SamplingSite *site : this->model->samplingSites) {
        for (MapNode *n : site->points) {
            int x0 = (int) zoom(n->x, centerX, viewW/2.0f, this->viewZoom);
            int y0 = (int) zoom(n->y, centerY, viewH/2.0f, -this->viewZoom);
            if (x0 < 0 || x0 > ((int) viewW) || y0 < 0 || y0 > ((int) viewH)) {
                continue;
            }
            dc.DrawCircle(x0, y0, 4);
        }
    }
    #ifdef DRAW_HYDRO_NODES
    dc.SetPen(*wxRED_PEN);
    dc.SetBrush(wxNullBrush);
    for (DistribHydroNode &n : this->model->hydroModel.hydroNodes) {
        int x0 = (int) zoom(n.x, centerX, viewW/2.0f, this->viewZoom);
        int y0 = (int) zoom(n.y, centerY, viewH/2.0f, -this->viewZoom);
        if (x0 < 0 || x0 > ((int) viewW) || y0 < 0 || y0 > ((int) viewH)) {
            continue;
        }
        dc.DrawCircle(x0, y0, 4);
    }
    #endif
}

void MapView::grab(wxMouseEvent &evt) {
    this->mouseGrabCoords = evt.GetPosition();
    this->preGrabCenterX = this->mapCenterX;
    this->preGrabCenterY = this->mapCenterY;
    this->isGrabbed = true;
}

void MapView::release(wxMouseEvent &evt) {
    this->isGrabbed = false;
}

void MapView::mouseMove(wxMouseEvent &evt) {
    if (this->isGrabbed) {
        int w; int h;
        this->GetClientSize(&w, &h);
        double dx = ((double) (evt.GetX() - this->mouseGrabCoords.x))/ (double) this->viewZoom;
        double dy = ((double) -(evt.GetY() - this->mouseGrabCoords.y))/ (double) this->viewZoom;
        this->mapCenterX = ((double) this->preGrabCenterX) - dx;
        this->mapCenterY = ((double) this->preGrabCenterY) - dy;
        this->Refresh();
    }
}

void MapView::selectNode(wxMouseEvent &evt) {
    int w; int h;
    this->GetClientSize(&w, &h);
    float x = unzoom((float) evt.GetX(), this->mapCenterX, ((float) w) / 2.0f, this->viewZoom);
    float y = unzoom((float) evt.GetY(), this->mapCenterY, ((float) h) / 2.0f, -this->viewZoom);
    this->selectedNode = nullptr;
    float minDistance;
    for (MapNode *n : this->model->map) {
        float dx = n->x - x;
        float dy = n->y - y;
        float distance = sqrt((dx*dx) + (dy*dy));
        if (this->selectedNode == nullptr || distance < minDistance) {
            minDistance = distance;
            this->selectedNode = n;
        }
    }
    this->updateDropdown();
    this->Refresh();
}

void MapView::updateDropdown() {
    this->fishSelector->Clear();
    if (this->selectedNode == nullptr) {
        return;
    }
    for (size_t i = 0; i < this->selectedNode->residentIds.size() && i < 25; ++i) {
        size_t id = this->selectedNode->residentIds[i];
        this->fishSelector->Append(std::to_string(id));
    }
}

void MapView::updateSelectedFishRange() {
    std::unordered_map<MapNode *, float> reachable;
    std::unordered_map<MapNode *, float> destinationProbs;
    Fish &selectedFish = this->model->individuals[this->selectedFishId];
    selectedFish.getReachableNodes(*this->model, reachable);
    selectedFish.getDestinationProbs(*this->model, destinationProbs);
    this->selectedFishRange.clear();
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        this->selectedFishRange[it->first] = 0.0;
        if (destinationProbs.count(it->first)) {
            this->selectedFishRange[it->first] = destinationProbs[it->first];
        }
    }
}

void MapView::selectFish(wxCommandEvent &evt) {
    int selection = this->fishSelector->GetCurrentSelection();
    this->selectedFishId = (long) this->selectedNode->residentIds[selection];
    this->tagButton->Enable(true);
    this->updateSelectedFishRange();
    this->Refresh();
}

void MapView::tagFish(wxCommandEvent &evt) {
    this->model->tagIndividual(this->selectedFishId);
}

void MapView::onModelUpdate() {
    if (this->selectedFishId != -1) {
        this->selectedNode = this->model->individuals[this->selectedFishId].location;
        this->updateSelectedFishRange();
    }
    this->updateDropdown();
    this->Refresh();
}

void MapView::onModelReset() {
    this->selectedNode = nullptr;
    this->selectedFishId = -1;
    this->fishSelector->Clear();
    this->Refresh();
}

class FishFrame : public wxFrame {
public:
    FishFrame(const wxString& title, const wxPoint& pos, const wxSize& size, const int argc, const wxCmdLineArgsArray &argv);
private:
    int updateRate;
    wxChoice *updateRateSelector;
    wxButton *updateButton;
    wxButton *forwardButton;
    wxButton *backButton;
    Model *model;
    MapView *view;
    bool replayMode;

    void setUpdateIncrement(wxCommandEvent &event);
    void updateModel(wxCommandEvent &event);
    void zoomIn(wxCommandEvent &event);
    void zoomOut(wxCommandEvent &event);
    void loadState(wxCommandEvent &event);
    void saveState(wxCommandEvent &event);
    void loadHistories(wxCommandEvent &event);
    void saveHistories(wxCommandEvent &event);
    void enterReplayMode();
    void stepForward(wxCommandEvent &event);
    void stepBackward(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(FishFrame, wxFrame)
    EVT_MENU(wxID_OPEN, FishFrame::loadState)
    EVT_MENU(wxID_SAVE, FishFrame::saveState)
wxEND_EVENT_TABLE()

FishFrame::FishFrame(const wxString &title, const wxPoint &pos, const wxSize& size, const int argc, const wxCmdLineArgsArray &argv)
    : wxFrame(nullptr, -1, title, pos, size), updateRate(1), replayMode(false)
{
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(wxID_OPEN);
    fileMenu->Append(wxID_SAVE);

    int loadHistoryMenuID = wxWindow::NewControlId();
    int saveHistoryMenuID = wxWindow::NewControlId();

    fileMenu->Append(loadHistoryMenuID, "Load tag replay");
    fileMenu->Append(saveHistoryMenuID, "Save tag replay");

    this->Bind(wxEVT_MENU, &FishFrame::loadHistories, this, loadHistoryMenuID);
    this->Bind(wxEVT_MENU, &FishFrame::saveHistories, this, saveHistoryMenuID);

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");

    SetMenuBar(menuBar);

    this->updateButton = new wxButton(this, -1, "Update");
    this->updateButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FishFrame::updateModel, this);

    this->forwardButton = new wxButton(this, -1, "Step Forward");
    this->forwardButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FishFrame::stepForward, this);
    this->forwardButton->Hide();

    this->backButton = new wxButton(this, -1, "Step Backward");
    this->backButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FishFrame::stepBackward, this);
    this->backButton->Hide();

    this->updateRateSelector = new wxChoice(this, -1);
    this->updateRateSelector->Append("1hr");
    this->updateRateSelector->Append("24hr");
    this->updateRateSelector->Append("1wk");
    this->updateRateSelector->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &FishFrame::setUpdateIncrement, this);

    wxButton *zoomInButton = new wxButton(this, -1, "Zoom In");
    zoomInButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FishFrame::zoomIn, this);
    wxButton *zoomOutButton = new wxButton(this, -1, "Zoom Out");
    zoomOutButton->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FishFrame::zoomOut, this);

    wxChoice *fishSelector = new wxChoice(this, -1);

    std::string configFile("default_config_env_from_file.json");
    if (argc > 1) {
        configFile = std::string(argv[1]);
    }

    try {
        this->model = modelFromConfig(configFile);
    }
    catch (std::runtime_error e) {
        std::cerr << e.what();
        this->Destroy();
        return;
    }

    wxButton *tagButton = new wxButton(this, -1, "Tag Fish");
    tagButton->Enable(false);

    this->view = new MapView(this, this->model, fishSelector, tagButton, 720, 640);

    wxBoxSizer *sizerVert = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *sizerHrz1 = new wxBoxSizer(wxHORIZONTAL);
    sizerHrz1->Add(this->updateButton, 4, wxEXPAND);
    sizerHrz1->Add(this->forwardButton, 2, wxEXPAND);
    sizerHrz1->Add(this->backButton, 2, wxEXPAND);
    sizerHrz1->Add(this->updateRateSelector, 1, wxEXPAND);
    sizerHrz1->Add(zoomInButton, 2, wxEXPAND);
    sizerHrz1->Add(zoomOutButton, 2, wxEXPAND);
    sizerHrz1->Add(fishSelector, 4, wxEXPAND);
    sizerHrz1->Add(tagButton, 1, wxEXPAND);
    wxBoxSizer *sizerHrz2 = new wxBoxSizer(wxHORIZONTAL);
    sizerHrz2->Add(this->view, 1, wxEXPAND);
    sizerVert->Add(sizerHrz1, 0, wxEXPAND);
    sizerVert->Add(sizerHrz2, 1, wxEXPAND);

    this->SetSizer(sizerVert);
    this->Show();
    this->SetClientSize(720, 640);
}

void FishFrame::setUpdateIncrement(wxCommandEvent &evt) {
    int selection = this->updateRateSelector->GetCurrentSelection();
    switch(selection) {
        case 0:
            this->updateRate = 1;
            return;
        case 1:
            this->updateRate = 24;
            return;
        case 2:
            this->updateRate = 24*7;
            return;
    }
}

void FishFrame::updateModel(wxCommandEvent &evt) {
    for (int i = 0; i < this->updateRate; ++i) {
        this->model->masterUpdate();
    }
    this->view->onModelUpdate();
}

void FishFrame::zoomIn(wxCommandEvent &evt) {
    this->view->viewZoom *= 1.5f;
    delete this->view->_buffer;
    this->view->_buffer = nullptr;
    this->view->Refresh();
}

void FishFrame::zoomOut(wxCommandEvent &evt) {
    this->view->viewZoom /= 1.5f;
    delete this->view->_buffer;
    this->view->_buffer = nullptr;
    this->view->Refresh();
}

void FishFrame::loadState(wxCommandEvent &evt) {
    wxFileDialog openFileDlg(this, "Open state file", "", "", "netCDF files (*.nc)|*.nc", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (openFileDlg.ShowModal() == wxID_CANCEL) {
        return;
    }
    this->model->loadState(openFileDlg.GetPath().ToStdString());
    //wxMessageBox("Invalid state file!", "Unable to load model state", wxICON_ERROR | wxOK, this);
    this->view->onModelReset();
}

void FishFrame::saveState(wxCommandEvent &evt) {
    wxFileDialog saveFileDlg(this, "Save state file", "", "state.nc", "netCDF files (*.nc)|*.nc", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
    if (saveFileDlg.ShowModal() == wxID_CANCEL) {
        return;
    }
    this->model->saveState(saveFileDlg.GetPath().ToStdString());
}

void FishFrame::loadHistories(wxCommandEvent &evt) {
    wxFileDialog openFileDlg(this, "Open tagged fish history file", "", "", "netCDF files (*.nc)|*.nc", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (openFileDlg.ShowModal() == wxID_CANCEL) {
        return;
    }
    this->model->loadTaggedHistories(openFileDlg.GetPath().ToStdString());
    this->enterReplayMode();
    this->view->onModelReset();
}

void FishFrame::saveHistories(wxCommandEvent &evt) {
    wxFileDialog saveFileDlg(this, "Save tagged fish history file", "", "tagged.nc", "netCDF files (*.nc)|*.nc", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
    if (saveFileDlg.ShowModal() == wxID_CANCEL) {
        return;
    }
    this->model->saveTaggedHistories(saveFileDlg.GetPath().ToStdString());
}

void FishFrame::enterReplayMode() {
    this->replayMode = true;
    this->updateButton->Hide();
    this->forwardButton->Show();
    this->backButton->Show();
    this->Layout();
}

void FishFrame::stepForward(wxCommandEvent &evt) {
    this->model->setHistoryTimestep(this->model->time + this->updateRate);
    this->view->onModelUpdate();
}

void FishFrame::stepBackward(wxCommandEvent &evt) {
    this->model->setHistoryTimestep(std::max(0L, this->model->time - this->updateRate));
    this->view->onModelUpdate();
}

wxIMPLEMENT_APP(FishGui);

bool FishGui::OnInit() {
    FishFrame *frame = new FishFrame("Skagit River Delta IBM", wxDefaultPosition, wxDefaultSize, this->argc, this->argv);
    frame->Show();
    return true;
}