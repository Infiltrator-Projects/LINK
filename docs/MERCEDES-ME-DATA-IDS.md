<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Mercedes me Adapter diagnostic data IDs

This catalogue preserves **194 exact static data-ID symbol/literal pairs**
recovered from the archived official Mercedes me Adapter 4.7.61 Android build.
The original 120-entry evidence set came principally from `classes3.dex`
SHA-256
`83cd980cac55e517926469f165cdd83f55eddec7c45e1590c45b6686c5685ae0`.
A later native-library pass recovered 74 additional exact `DataIds::DATAID_*`
exports whose lower-camel literals also occur verbatim in the supplied
`libdiaglogic.so`. Two exported native symbols whose exact application
literal was not independently established were deliberately not added.

These are model/protocol identifiers, not proof of the underlying CAN/UDS DID,
byte offset or scaling. A key appearing here proves that the official
diagnostic framework knew that value; it does not prove that every supported
vehicle or adapter generation returns it.

| Constant | Exact application data ID / literal |
| --- | --- |
| `ACCELERATION` | `acceleration` |
| `ACTUAL_ENGINE_TORQUE` | `actualEngineTorque` |
| `AD_BLUE_REMAINING_DISTANCE` | `adBlueRemainingDistanceFiltered2` |
| `AMBIENT_AIR_TEMPERATURE` | `ambientAirTemperature` |
| `BACKUP_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `backupLampLeftFaultOccurredOnTrip` |
| `BACKUP_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `backupLampRightFaultOccurredOnTrip` |
| `BATTERY_VOLTAGE` | `batteryVoltage` |
| `BATTERY_VOLTAGE_CRITICAL` | `batteryVoltageCritical` |
| `BATTERY_VOLTAGE_CRITICAL_OCCURRED_ON_TRIP` | `batteryVoltageCriticalOccurredOnTrip` |
| `BRAKE_FLUID_LEVEL_CRITICAL` | `brakeFluidLevelCritical` |
| `BRAKE_FLUID_LEVEL_CRITICAL_OCCURRED_ON_TRIP` | `brakeFluidLevelCriticalOccurredOnTrip` |
| `BRAKE_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `brakeLampLeftFaultOccurredOnTrip` |
| `BRAKE_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `brakeLampRightFaultOccurredOnTrip` |
| `BRAKE_LAMP3_FAULT_OCCURRED_ON_TRIP` | `brakeLamp3FaultOccurredOnTrip` |
| `BRAKE_LINING_CRITICAL` | `brakeLiningCritical` |
| `BRAKE_LINING_CRITICAL_OCCURRED_ON_TRIP` | `brakeLiningCriticalOccurredOnTrip` |
| `BRAKE_TAIL_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `brakeTailLampLeftFaultOccurredOnTrip` |
| `BRAKE_TAIL_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `brakeTailLampRightFaultOccurredOnTrip` |
| `BT_RX_OVERFLOW_COUNT` | `btRxOverflowCount` |
| `BT_TX_OVERFLOW_COUNT` | `btTxOverflowCount` |
| `BUS_ERROR_COUNT` | `busErrorCount` |
| `CALCULATED_ENGINE_LOAD` | `calculatedEngineLoad` |
| `CAN_RX_OVERFLOW_COUNT` | `canRxOverflowCount` |
| `CAN_TX_OVERFLOW_COUNT` | `canTxOverflowCount` |
| `DATA_POINT_LOOP_ENDED` | `dataPointLoopEnded` |
| `DAYTIME_RUNNING_LAMPS_LEFT_FAULT_OCCURRED_ON_TRIP` | `daytimeRunningLampsLeftFaultOccurredOnTrip` |
| `DAYTIME_RUNNING_LAMPS_RIGHT_FAULT_OCCURRED_ON_TRIP` | `daytimeRunningLampsRightFaultOccurredOnTrip` |
| `DCS_ECU_IDENTIFICATION` | `dcsEcuIdentification` |
| `DIAG_VARIANT_IDENTIFICATION` | `diagVariantIdentification` |
| `DISTANCE_TRAVELED_SINCE_CODES_CLEARED` | `distanceTraveledSinceCodesCleared` |
| `ECU_BOOT_SW_IDENTIFICATION` | `ecuBootSwIdentification` |
| `ECU_CODE_FINGERPRINT_IDENTIFICATION` | `ecuCodeFingerprintIdentification` |
| `ECU_CODE_SW_IDENTIFICATION` | `ecuCodeSwIdentification` |
| `ECU_DATA_FINGERPRINT_IDENTIFICATION` | `ecuDataFingerprintIdentification` |
| `ECU_DATA_SW_IDENTIFICATION` | `ecuDataSwIdentification` |
| `ECU_SERIAL_NUMBER_IDENTIFICATION` | `ecuSerialNumberIdentification` |
| `EMERGENCY_FLASHER_FAULT_OCCURRED_ON_TRIP` | `emergencyFlasherFaultOccurredOnTrip` |
| `EMERGENCY_LIGHT_PATH_FAULT_BC_F_OCCURRED_ON_TRIP` | `emergencyLightPathFaultBcFOccurredOnTrip` |
| `EMERGENCY_LIGHT_PATH_FAULT_SAM_F_OCCURRED_ON_TRIP` | `emergencyLightPathFaultSamFOccurredOnTrip` |
| `EMERGENCY_LIGHT_PATH_FAULT_SAM_R_OCCURRED_ON_TRIP` | `emergencyLightPathFaultSamROccurredOnTrip` |
| `ENGINE_COOLANT_FLUID_LEVEL_CRITICAL` | `engineCoolantFluidLevelCritical` |
| `ENGINE_COOLANT_FLUID_LEVEL_CRITICAL_OCCURRED_ON_TRIP` | `engineCoolantFluidLevelCriticalOccurredOnTrip` |
| `ENGINE_COOLANT_TEMPERATURE` | `engineCoolantTemperature` |
| `ENGINE_COOLANT_TEMPERATURE_UNIT` | `C` |
| `ENGINE_FUEL_RATE` | `engineFuelRate` |
| `ENGINE_OIL_PRESSURE_CRITICAL` | `engineOilPressureCritical` |
| `ENGINE_OIL_TEMPERATURE` | `engineOilTemperature` |
| `ENGINE_REFERENCE_THROTTLE` | `engineReferenceThrottle` |
| `ENGINE_REFERENCE_TORQUE` | `engineReferenceTorque` |
| `ENGINE_RPM` | `engineRpm` |
| `FLICKERING_BLUETOOTH_CONNECTION_COUNT` | `flickeringBluetoothConnectionCount` |
| `FOG_LAMP_FRONT_LEFT_FAULT_OCCURRED_ON_TRIP` | `fogLampFrontLeftFaultOccurredOnTrip` |
| `FOG_LAMP_REAR_LEFT_FAULT_OCCURRED_ON_TRIP` | `fogLampRearLeftFaultOccurredOnTrip` |
| `FOG_LAMP_REAR_RIGHT_FAULT_OCCURRED_ON_TRIP` | `fogLampRearRightFaultOccurredOnTrip` |
| `FOG_LIGHT_FRONT_RIGHT_FAULT_OCCURRED_ON_TRIP` | `fogLightFrontRightFaultOccurredOnTrip` |
| `FUEL_LEVEL_MIN` | `fuelLevelMin` |
| `FUEL_LEVEL_PERCENTAGE_UNIT` | `%` |
| `FUEL_PRESSURE` | `fuelPressure` |
| `FUEL_VALUE_UNIT` | `l` |
| `FUEL_VOLUME` | `fuelVolume` |
| `FUEL_VOLUME_STATISTICS` | `fuelVolumeStatistics` |
| `GENERAL_MALFUNCTION_INDICATED` | `generalMalfunctionIndicated` |
| `GENERAL_MALFUNCTION_INDICATED_OCCURRED_ON_TRIP` | `generalMalfunctionIndicatedOccurredOnTrip` |
| `HIGH_BEAM_LEFT_FAULT_OCCURRED_ON_TRIP` | `highBeamLeftFaultOccurredOnTrip` |
| `HIGH_BEAM_RIGHT_FAULT_OCCURRED_ON_TRIP` | `highBeamRightFaultOccurredOnTrip` |
| `IGNITION_OFF_VOLTAGE_THRESHOLD` | `ignitionOffVoltageThreshold` |
| `IGNITION_STATE_BOOLEAN` | `ignitionStateBoolean` |
| `IGNITION_STATE_RAW` | `ignitionStateRaw` |
| `INFRARED_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `infraredLampLeftFaultOccurredOnTrip` |
| `INFRARED_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `infraredLampRightFaultOccurredOnTrip` |
| `INTAKE_AIR_TEMPERATURE` | `intakeAirTemperature` |
| `INTAKE_MANIFOLD_PRESSURE` | `intakeManifoldPressure` |
| `LICENSE_PLATE_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `licensePlateLampLeftFaultOccurredOnTrip` |
| `LICENSE_PLATE_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `licensePlateLampRightFaultOccurredOnTrip` |
| `LIVE_DATA_AVAILABILITY_DATA_IDS_TO_READ` | `liveDataAvailabilityDataIdsToRead` |
| `LIVE_DATA_AVAILABILITY_DATA_IDS_TO_READ_COUNT` | `liveDataAvailabilityDataIdsToReadCount` |
| `LOW_BEAM_LEFT_FAULT_OCCURRED_ON_TRIP` | `lowBeamLeftFaultOccurredOnTrip` |
| `LOW_BEAM_RIGHT_FAULT_OCCURRED_ON_TRIP` | `lowBeamRightFaultOccurredOnTrip` |
| `MAINTENANCE_PRIO_RESIDUAL` | `maintenancePrioResidual` |
| `MAINTENANCE_RAW1` | `maintenanceRaw1` |
| `MAINTENANCE_RAW2` | `maintenanceRaw2` |
| `MAINTENANCE_REMAINING_DISTANCE` | `maintenanceRemainingDistance` |
| `MAINTENANCE_REMAINING_DISTANCE_IN_MILES` | `maintenanceRemainingDistanceInMiles` |
| `MAINTENANCE_REMAINING_TIME` | `maintenanceRemainingTime` |
| `MAINTENANCE_SERVICE_INTERVAL_DAY` | `maintenanceServiceIntervalDay` |
| `MAINTENANCE_SERVICE_INTERVAL_DAY_UNIT` | `Day` |
| `MAINTENANCE_SERVICE_INTERVAL_KM` | `maintenanceServiceIntervalKm` |
| `MAINTENANCE_SERVICE_INTERVAL_KM_UNIT` | `km` |
| `MAX_FLICKERING_BLUETOOTH_CONNECTION_COUNT` | `maxFlickeringBluetoothConnectionCount` |
| `MAXIMUM_BATTERY_VOLTAGE` | `maximumBatteryVoltage` |
| `MEASURED_MILEAGE` | `measuredMileage` |
| `MILEAGE` | `mileage` |
| `MILEAGE_DOUBLE` | `mileageDouble` |
| `MILEAGE_OF_LAST_FUEL_READ` | `mileageOfLastFuelRead` |
| `MILEAGE_UNIT` | `km` |
| `MMC_ECU_IDENTIFICATION` | `mmcEcuIdentification` |
| `OBD_ADAPTER_VOLTAGE_V_UNIT` | `V` |
| `PARKING_LAMP_FRONT_LEFT_FAULT_OCCURRED_ON_TRIP` | `parkingLampFrontLeftFaultOccurredOnTrip` |
| `PARKING_LAMP_FRONT_RIGHT_FAULT_OCCURRED_ON_TRIP` | `parkingLampFrontRightFaultOccurredOnTrip` |
| `RELATIVE_ACCELERATOR_PEDAL_POSITION` | `relativeAcceleratorPedalPosition` |
| `SCORING_AVAILABLE` | `scoringAvailable` |
| `SIDEMARKER_FRONT_LEFT_FAULT_OCCURRED_ON_TRIP` | `sidemarkerFrontLeftFaultOccurredOnTrip` |
| `SIDEMARKER_FRONT_RIGHT_FAULT_OCCURRED_ON_TRIP` | `sidemarkerFrontRightFaultOccurredOnTrip` |
| `SIDEMARKER_REAR_LEFT_FAULT_OCCURRED_ON_TRIP` | `sidemarkerRearLeftFaultOccurredOnTrip` |
| `SIDEMARKER_REAR_RIGHT_FAULT_OCCURRED_ON_TRIP` | `sidemarkerRearRightFaultOccurredOnTrip` |
| `TAIL_LAMP_LEFT_FAULT_OCCURRED_ON_TRIP` | `tailLampLeftFaultOccurredOnTrip` |
| `TAIL_LAMP_RIGHT_FAULT_OCCURRED_ON_TRIP` | `tailLampRightFaultOccurredOnTrip` |
| `TANK_RANGE` | `tankRange` |
| `THROTTLE_POSITION` | `throttlePosition` |
| `TIRE_PRESSURE_CRITICAL` | `tirePressureCritical` |
| `TIRE_PRESSURE_CRITICAL_OCCURRED_ON_TRIP` | `tirePressureCriticalOccurredOnTrip` |
| `TRIP_AVERAGE_SPEED` | `tripAverageSpeed` |
| `TRIP_START_MILEAGE_DOUBLE` | `tripStartMileageDouble` |
| `TURN_INDICATION_LAMP_FRONT_LEFT_FAULT_OCCURRED_ON_TRIP` | `turnIndicationLampFrontLeftFaultOccurredOnTrip` |
| `TURN_INDICATION_LAMP_FRONT_RIGHT_FAULT_OCCURRED_ON_TRIP` | `turnIndicationLampFrontRightFaultOccurredOnTrip` |
| `TURN_INDICATION_LAMP_REAR_LEFT_FAULT_OCCURRED_ON_TRIP` | `turnIndicationLampRearLeftFaultOccurredOnTrip` |
| `TURN_INDICATION_LAMP_REAR_RIGHT_FAULT_OCCURRED_ON_TRIP` | `turnIndicationLampRearRightFaultOccurredOnTrip` |
| `TURN_INDICATION_LAMPS_REAR_FAULT_OCCURRED_ON_TRIP` | `turnIndicationLampsRearFaultOccurredOnTrip` |
| `TURN_INDICATION_LAMPS_REAR_FAULT_TM_OCCURRED_ON_TRIP` | `turnIndicationLampsRearFaultTmOccurredOnTrip` |
| `VEHICLE_SPEED` | `vehicleSpeed` |

Particularly relevant to current MBLINK work are the separate
`relativeAcceleratorPedalPosition`, `throttlePosition` and
`engineReferenceThrottle` identities, which confirm that the official
framework did not conflate accelerator position with throttle-valve position.
The fuel group also separately identifies `fuelVolume`, `fuelLevelMin`,
`fuelPressure`, `engineFuelRate` and `tankRange`. The adapter-health group
additionally exposes `btRxOverflowCount`, `btTxOverflowCount`,
`canRxOverflowCount`, `canTxOverflowCount` and `busErrorCount`.


## Additional native DiagLogic pairs

The 74 later additions expose aggregate read groups and non-trip fault states
that were absent from the first DEX-only catalogue. High-value exact pairs
include `STORED_OBD_DTCS -> storedObdDtcs`,
`PARTICLE_FILTER -> particleFilter`,
`ACTUAL_FUEL_FLOW -> actualFuelFlow`,
`FUEL_FLOW_VALUES -> fuelFlowValues`,
`SPEED_AND_FUEL_VALUES -> speedAndFuelValues` and
`IRREGULAR_OBD_RESPONSE -> irregularObdResponse`.

These additions remain model identifiers only. They improve the implementation
checklist but do not create a CAN/UDS/KWP request, payload or scaling mapping.
