#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include "raylib.h"
#include "component_base.h"
#include <stdbool.h>

// Event types
typedef enum {
    EVENT_NONE = 0,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_DOUBLE_CLICK,
    EVENT_MOUSE_WHEEL,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_KEY_PRESS,
    EVENT_TEXT_INPUT,
    EVENT_FOCUS_GAIN,
    EVENT_FOCUS_LOSE,
    EVENT_HOVER_ENTER,
    EVENT_HOVER_LEAVE,
    EVENT_DRAG_START,
    EVENT_DRAG_MOVE,
    EVENT_DRAG_END,
    EVENT_RESIZE,
    EVENT_MOVE,
    EVENT_SHOW,
    EVENT_HIDE,
    EVENT_ENABLE,
    EVENT_DISABLE,
    EVENT_VALUE_CHANGE,
    EVENT_SELECTION_CHANGE,
    EVENT_CUSTOM
} EventType;

// Event data union
typedef union {
    Vector2 mouse_position;
    int key_code;
    char text_input[256];
    Rectangle rect;
    float value;
    int selection_index;
    void* custom_data;
} EventData;

// Event structure
typedef struct {
    EventType type;
    EventData data;
    ComponentBase* target;
    ComponentBase* source;
    unsigned int timestamp;
    bool handled;
    bool bubbles;
    bool cancelable;
} Event;

// Event listener function type
typedef bool (*EventListener)(Event* event, void* user_data);

// Event listener registration
typedef struct {
    EventType event_type;
    EventListener listener;
    void* user_data;
    bool once;
    int priority;
} EventListenerRegistration;

// Event target (component that can receive events)
typedef struct {
    ComponentBase* component;
    EventListenerRegistration* listeners;
    int listener_count;
    int listener_capacity;
    bool event_enabled;
} EventTarget;

// Event system context
typedef struct {
    EventTarget* targets;
    int target_count;
    int target_capacity;
    Event* event_queue;
    int queue_head;
    int queue_tail;
    int queue_size;
    int queue_capacity;
    bool processing_events;
} EventSystem;

// Event system functions
bool EventSystem_Init(EventSystem* system, int queue_capacity);
void EventSystem_Destroy(EventSystem* system);

// Event target management
bool EventSystem_RegisterTarget(EventSystem* system, ComponentBase* component);
bool EventSystem_UnregisterTarget(EventSystem* system, ComponentBase* component);
EventTarget* EventSystem_GetTarget(EventSystem* system, ComponentBase* component);

// Event listener management
bool EventSystem_AddEventListener(EventSystem* system, ComponentBase* component, EventType event_type, EventListener listener, void* user_data);
bool EventSystem_AddEventListenerOnce(EventSystem* system, ComponentBase* component, EventType event_type, EventListener listener, void* user_data);
bool EventSystem_AddEventListenerWithPriority(EventSystem* system, ComponentBase* component, EventType event_type, EventListener listener, void* user_data, int priority);
bool EventSystem_RemoveEventListener(EventSystem* system, ComponentBase* component, EventType event_type, EventListener listener);
bool EventSystem_RemoveAllEventListeners(EventSystem* system, ComponentBase* component);
bool EventSystem_RemoveAllEventListenersByType(EventSystem* system, ComponentBase* component, EventType event_type);

// Event dispatching
bool EventSystem_DispatchEvent(EventSystem* system, Event* event);
bool EventSystem_QueueEvent(EventSystem* system, Event* event);
bool EventSystem_ProcessEventQueue(EventSystem* system);
bool EventSystem_ClearEventQueue(EventSystem* system);

// Event creation helpers
Event Event_CreateMouseEvent(EventType type, Vector2 position, ComponentBase* target);
Event Event_CreateKeyEvent(EventType type, int key_code, ComponentBase* target);
Event Event_CreateTextEvent(const char* text, ComponentBase* target);
Event Event_CreateFocusEvent(EventType type, ComponentBase* target);
Event Event_CreateValueEvent(float value, ComponentBase* target);
Event Event_CreateSelectionEvent(int selection_index, ComponentBase* target);
Event Event_CreateCustomEvent(void* custom_data, ComponentBase* target);

// Event handling
bool Event_StopPropagation(Event* event);
bool Event_PreventDefault(Event* event);
bool Event_IsHandled(const Event* event);
bool Event_IsBubbling(const Event* event);
bool Event_IsCancelable(const Event* event);

// Event bubbling and capturing
bool EventSystem_DispatchEventWithBubbling(EventSystem* system, Event* event);
bool EventSystem_DispatchEventWithCapturing(EventSystem* system, Event* event);

// Event system state
bool EventSystem_IsProcessingEvents(const EventSystem* system);
int EventSystem_GetQueueSize(const EventSystem* system);
int EventSystem_GetTargetCount(const EventSystem* system);

// Event debugging
void EventSystem_DebugPrintEvent(const Event* event);
void EventSystem_DebugPrintTarget(const EventTarget* target);
void EventSystem_DebugPrintSystem(const EventSystem* system);

// Utility functions
const char* Event_TypeToString(EventType type);
EventType Event_StringToType(const char* type_string);
unsigned int Event_GetTimestamp(void);

#endif // EVENT_SYSTEM_H
