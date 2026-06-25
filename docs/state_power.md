# State Machine: Power Management

## States
ACTIVE → CONNECTED_IDLE → DISCONNECTED_BUFFERING → LOW_BATTERY → SLEEP → RECOVERY

## Diagram
       power on / boot
             │
             ▼
       ┌───────────┐
       │  ACTIVE   │◄────────────────────────────────┐
       └─────┬─────┘                                 │
             │                                       │
     ┌───────┼──────────────────┐                    │
     │       │                  │                    │
     │ No activity (10m)        │ WiFi lost          │ Battery > 20%
     ▼       ▼                  ▼                    │ (recovered)
┌──────────────┐      ┌─────────────────────────┐    │
│CONNECTED_IDLE│      │DISCONNECTED_BUFFERING  │    │
└────────┬─────┘      └─────────────────────────┘    │
         │                          │                │
         │ battery <= 20%           │ battery <= 20% │
         └───────────┐  ┌───────────┘                │
                     ▼  ▼                            │
               ┌─────────────┐                       │
               │ LOW_BATTERY ├───────────────────────┘
               └──────┬──────┘
                      │
                      ├───────── repeated boot loops / brownout loop ──────┐
                      │                                                   ▼
                      │ Inactivity / Sleep command                  ┌──────────┐
                      ▼                                             │ RECOVERY │
                  ┌───────┐                                         └──────────┘
                  │ SLEEP │                                               ▲
                  └───────┘                                               │
                      ▲                                            critical system
                      │ WakeReason (Button/Timer)                  fault / dead cell
                      └───────┘                                    ───────────────┘


                      From,To,Guard,Action
ACTIVE,CONNECTED_IDLE,Inactivity > 10 mins,"Dim UI, enter light regulation"
ACTIVE,DISCONNECTED_BUFFERING,WiFi connection lost,"Throttle SyncTask, start local queue buffering"
CONNECTED_IDLE,ACTIVE,Button press / interaction,"Restore full UI brightness, resume active tasks"
ANY STATE,LOW_BATTERY,Battery <= 20%,"Emit power_low event, blink warning LED, abort active OTA"
LOW_BATTERY,ACTIVE,Battery > 20% (charging),Resume normal operations and OTA checks
LOW_BATTERY,SLEEP,System idle threshold,"Save registers, enter deep sleep state"
LOW_BATTERY,RECOVERY,Repeated brownout/boot loops,"Lock device, flash red alert LED, disable radio entirely"
SLEEP,ACTIVE,WakeReason::BUTTON_PRESS / Timer,Trigger boot sequence validation