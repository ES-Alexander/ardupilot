#include "Copter.h"

/*
 * High level calls to set and update flight modes logic for individual
 * flight modes is in control_acro.cpp, control_stabilize.cpp, etc
 */

/*
  constructor for Mode object
 */
Mode::Mode(void) :
    g(copter.g),
    g2(copter.g2),
    wp_nav(copter.wp_nav),
    loiter_nav(copter.loiter_nav),
    pos_control(copter.pos_control),
    ahrs(copter.ahrs),
    attitude_control(copter.attitude_control),
    motors(copter.motors),
    channel_roll(copter.channel_roll),
    channel_pitch(copter.channel_pitch),
    channel_throttle(copter.channel_throttle),
    channel_yaw(copter.channel_yaw),
    G_Dt(copter.G_Dt)
{ };

#if AC_PAYLOAD_PLACE_ENABLED
PayloadPlace Mode::payload_place;
#endif

// return the static controller object corresponding to supplied mode
Mode *Copter::mode_from_mode_num(const Mode::Number mode)
{

    switch (mode) {
#if MODE_ACRO_ENABLED
        case Mode::Number::ACRO:
            return &mode_acro;
#endif

        case Mode::Number::STABILIZE:
            return &mode_stabilize;

        case Mode::Number::ALT_HOLD:
            return &mode_althold;

#if MODE_AUTO_ENABLED
        case Mode::Number::AUTO:
            return &mode_auto;
#endif

#if MODE_CIRCLE_ENABLED
        case Mode::Number::CIRCLE:
            return &mode_circle;
#endif

#if MODE_LOITER_ENABLED
        case Mode::Number::LOITER:
            return &mode_loiter;
#endif

#if MODE_GUIDED_ENABLED
        case Mode::Number::GUIDED:
            return &mode_guided;
#endif

        case Mode::Number::LAND:
            return &mode_land;

#if MODE_RTL_ENABLED
        case Mode::Number::RTL:
            return &mode_rtl;
#endif

#if MODE_DRIFT_ENABLED
        case Mode::Number::DRIFT:
            return &mode_drift;
#endif

#if MODE_SPORT_ENABLED
        case Mode::Number::SPORT:
            return &mode_sport;
#endif

#if MODE_FLIP_ENABLED
        case Mode::Number::FLIP:
            return &mode_flip;
#endif

#if AUTOTUNE_ENABLED
        case Mode::Number::AUTOTUNE:
            return &mode_autotune;
#endif

#if MODE_POSHOLD_ENABLED
        case Mode::Number::POSHOLD:
            return &mode_poshold;
#endif

#if MODE_BRAKE_ENABLED
        case Mode::Number::BRAKE:
            return &mode_brake;
#endif

#if MODE_THROW_ENABLED
        case Mode::Number::THROW:
            return &mode_throw;
#endif

#if AP_ADSB_AVOIDANCE_ENABLED
        case Mode::Number::AVOID_ADSB:
            return &mode_avoid_adsb;
#endif

#if MODE_GUIDED_NOGPS_ENABLED
        case Mode::Number::GUIDED_NOGPS:
            return &mode_guided_nogps;
#endif

#if MODE_SMARTRTL_ENABLED
        case Mode::Number::SMART_RTL:
            return &mode_smartrtl;
#endif

#if MODE_FLOWHOLD_ENABLED
        case Mode::Number::FLOWHOLD:
            return (Mode *)g2.mode_flowhold_ptr;
#endif

#if MODE_FOLLOW_ENABLED
        case Mode::Number::FOLLOW:
            return &mode_follow;
#endif

#if MODE_ZIGZAG_ENABLED
        case Mode::Number::ZIGZAG:
            return &mode_zigzag;
#endif

#if MODE_SYSTEMID_ENABLED
        case Mode::Number::SYSTEMID:
            return (Mode *)g2.mode_systemid_ptr;
#endif

#if MODE_AUTOROTATE_ENABLED
        case Mode::Number::AUTOROTATE:
            return &mode_autorotate;
#endif

#if MODE_TURTLE_ENABLED
        case Mode::Number::TURTLE:
            return &mode_turtle;
#endif

        default:
            break;
    }

#if MODE_GUIDED_ENABLED && AP_SCRIPTING_ENABLED
    // Check registered custom modes
    for (uint8_t i = 0; i < ARRAY_SIZE(mode_guided_custom); i++) {
        if ((mode_guided_custom[i] != nullptr) && (mode_guided_custom[i]->mode_number() == mode)) {
            return mode_guided_custom[i];
        }
    }
#endif

    return nullptr;
}


// called when an attempt to change into a mode is unsuccessful:
void Copter::mode_change_failed(const Mode *mode, const char *reason)
{
    gcs().send_text(MAV_SEVERITY_WARNING, "Mode change to %s failed: %s", mode->name(), reason);
    LOGGER_WRITE_ERROR(LogErrorSubsystem::FLIGHT_MODE, LogErrorCode(mode->mode_number()));
    // make sad noise
    if (copter.ap.initialised) {
        AP_Notify::events.user_mode_change_failed = 1;
    }
}

// Check if this mode can be entered from the GCS
bool Copter::gcs_mode_enabled(const Mode::Number mode_num)
{
    // List of modes that can be blocked, index is bit number in parameter bitmask
    static const uint8_t mode_list [] {
        (uint8_t)Mode::Number::STABILIZE,
        (uint8_t)Mode::Number::ACRO,
        (uint8_t)Mode::Number::ALT_HOLD,
        (uint8_t)Mode::Number::AUTO,
        (uint8_t)Mode::Number::GUIDED,
        (uint8_t)Mode::Number::LOITER,
        (uint8_t)Mode::Number::CIRCLE,
        (uint8_t)Mode::Number::DRIFT,
        (uint8_t)Mode::Number::SPORT,
        (uint8_t)Mode::Number::FLIP,
        (uint8_t)Mode::Number::AUTOTUNE,
        (uint8_t)Mode::Number::POSHOLD,
        (uint8_t)Mode::Number::BRAKE,
        (uint8_t)Mode::Number::THROW,
        (uint8_t)Mode::Number::AVOID_ADSB,
        (uint8_t)Mode::Number::GUIDED_NOGPS,
        (uint8_t)Mode::Number::SMART_RTL,
        (uint8_t)Mode::Number::FLOWHOLD,
        (uint8_t)Mode::Number::FOLLOW,
        (uint8_t)Mode::Number::ZIGZAG,
        (uint8_t)Mode::Number::SYSTEMID,
        (uint8_t)Mode::Number::AUTOROTATE,
        (uint8_t)Mode::Number::AUTO_RTL,
        (uint8_t)Mode::Number::TURTLE
    };

    if (!block_GCS_mode_change((uint8_t)mode_num, mode_list, ARRAY_SIZE(mode_list))) {
        return true;
    }

    // Mode disabled, try and grab a mode name to give a better warning.
    Mode *new_flightmode = mode_from_mode_num(mode_num);
    if (new_flightmode != nullptr) {
        mode_change_failed(new_flightmode, "GCS entry disabled (FLTMODE_GCSBLOCK)");
    } else {
        notify_no_such_mode((uint8_t)mode_num);
    }

    return false;
}

// set_mode - change flight mode and perform any necessary initialisation
// optional force parameter used to force the flight mode change (used only first time mode is set)
// returns true if mode was successfully set
// ACRO, STABILIZE, ALTHOLD, LAND, DRIFT and SPORT can always be set successfully but the return state of other flight modes should be checked and the caller should deal with failures appropriately
bool Copter::set_mode(Mode::Number mode, ModeReason reason)
{
    // update last reason
    const ModeReason last_reason = _last_reason;
    _last_reason = reason;

    // return immediately if we are already in the desired mode
    if (mode == flightmode->mode_number()) {
        control_mode_reason = reason;
        // set yaw rate time constant during autopilot startup
        if (reason == ModeReason::INITIALISED && mode == Mode::Number::STABILIZE) {
            attitude_control->set_yaw_rate_tc(g2.command_model_pilot_y.get_rate_tc());
        }
        // make happy noise
        if (copter.ap.initialised && (reason != last_reason)) {
            AP_Notify::events.user_mode_change = 1;
        }
        return true;
    }

    // Check if GCS mode change is disabled via parameter
    if ((reason == ModeReason::GCS_COMMAND) && !gcs_mode_enabled(mode)) {
        return false;
    }

#if MODE_AUTO_ENABLED
    if (mode == Mode::Number::AUTO_RTL) {
        // Special case for AUTO RTL, not a true mode, just AUTO in disguise
        // Attempt to join return path, fallback to do-land-start
        return mode_auto.return_path_or_jump_to_landing_sequence_auto_RTL(reason);
    }
#endif

    Mode *new_flightmode = mode_from_mode_num(mode);
    if (new_flightmode == nullptr) {
        notify_no_such_mode((uint8_t)mode);
        return false;
    }

    bool ignore_checks = !motors->armed();   // allow switching to any mode if disarmed.  We rely on the arming check to perform

#if FRAME_CONFIG == HELI_FRAME
    // do not allow helis to enter a non-manual throttle mode if the
    // rotor runup is not complete
    if (!ignore_checks && !new_flightmode->has_manual_throttle() && !motors->rotor_runup_complete()) {
        mode_change_failed(new_flightmode, "runup not complete");
        return false;
    }
#endif

#if FRAME_CONFIG != HELI_FRAME
    // ensure vehicle doesn't leap off the ground if a user switches
    // into a manual throttle mode from a non-manual-throttle mode
    // (e.g. user arms in guided, raises throttle to 1300 (not enough to
    // trigger auto takeoff), then switches into manual):
    bool user_throttle = new_flightmode->has_manual_throttle();
#if MODE_DRIFT_ENABLED
    if (new_flightmode == &mode_drift) {
        user_throttle = true;
    }
#endif
    if (!ignore_checks &&
        ap.land_complete &&
        user_throttle &&
        !copter.flightmode->has_manual_throttle() &&
        new_flightmode->get_pilot_desired_throttle() > copter.get_non_takeoff_throttle()) {
        mode_change_failed(new_flightmode, "throttle too high");
        return false;
    }
#endif

    if (!ignore_checks &&
        new_flightmode->requires_GPS() &&
        !copter.position_ok()) {
        mode_change_failed(new_flightmode, "requires position");
        return false;
    }

    // check for valid altitude if old mode did not require it but new one does
    // we only want to stop changing modes if it could make things worse
    if (!ignore_checks &&
        !copter.ekf_alt_ok() &&
        flightmode->has_manual_throttle() &&
        !new_flightmode->has_manual_throttle()) {
        mode_change_failed(new_flightmode, "need alt estimate");
        return false;
    }

#if AP_FENCE_ENABLED
    // may not be allowed to change mode if recovering from fence breach
    if (!ignore_checks &&
        fence.enabled() &&
        fence.option_enabled(AC_Fence::OPTIONS::DISABLE_MODE_CHANGE) &&
        fence.get_breaches() &&
        motors->armed() &&
        get_control_mode_reason() == ModeReason::FENCE_BREACHED &&
        !ap.land_complete) {
        mode_change_failed(new_flightmode, "in fence recovery");
        return false;
    }
#endif

    if (rc().in_rc_failsafe() && !new_flightmode->allows_entry_in_rc_failsafe()) {
        mode_change_failed(new_flightmode, "in RC failsafe");
        return false;
    }

    if (!new_flightmode->init(ignore_checks)) {
        mode_change_failed(new_flightmode, "init failed");
        return false;
    }

    // perform any cleanup required by previous flight mode
    exit_mode(flightmode, new_flightmode);

    // update flight mode
    flightmode = new_flightmode;
    control_mode_reason = reason;
#if HAL_LOGGING_ENABLED
    logger.Write_Mode((uint8_t)flightmode->mode_number(), reason);
#endif
    gcs().send_message(MSG_HEARTBEAT);

#if HAL_ADSB_ENABLED
    adsb.set_is_auto_mode((mode == Mode::Number::AUTO) || (mode == Mode::Number::RTL) || (mode == Mode::Number::GUIDED));
#endif

#if AP_FENCE_ENABLED
    if (fence.get_action() != AC_Fence::Action::REPORT_ONLY) {
        // pilot requested flight mode change during a fence breach indicates pilot is attempting to manually recover
        // this flight mode change could be automatic (i.e. fence, battery, GPS or GCS failsafe)
        // but it should be harmless to disable the fence temporarily in these situations as well
        fence.manual_recovery_start();
    }
#endif

#if AP_CAMERA_ENABLED
    camera.set_is_auto_mode(flightmode->mode_number() == Mode::Number::AUTO);
#endif

    // set rate shaping time constants
#if MODE_ACRO_ENABLED || MODE_SPORT_ENABLED
    attitude_control->set_roll_pitch_rate_tc(g2.command_model_acro_rp.get_rate_tc());
#endif
    attitude_control->set_yaw_rate_tc(g2.command_model_pilot_y.get_rate_tc());
#if MODE_ACRO_ENABLED || MODE_DRIFT_ENABLED
    if (mode== Mode::Number::ACRO || mode== Mode::Number::DRIFT) {
        attitude_control->set_yaw_rate_tc(g2.command_model_acro_y.get_rate_tc());
    }
#endif

    // update notify object
    notify_flight_mode();

    // make happy noise
    if (copter.ap.initialised) {
        AP_Notify::events.user_mode_change = 1;
    }

    // return success
    return true;
}

bool Copter::set_mode(const uint8_t new_mode, const ModeReason reason)
{
    static_assert(sizeof(Mode::Number) == sizeof(new_mode), "The new mode can't be mapped to the vehicles mode number");
#ifdef DISALLOW_GCS_MODE_CHANGE_DURING_RC_FAILSAFE
    if (reason == ModeReason::GCS_COMMAND && copter.failsafe.radio) {
        // don't allow mode changes while in radio failsafe
        return false;
    }
#endif
    return copter.set_mode(static_cast<Mode::Number>(new_mode), reason);
}

// update_flight_mode - calls the appropriate attitude controllers based on flight mode
// called at 100hz or more
void Copter::update_flight_mode()
{
#if AP_RANGEFINDER_ENABLED
    surface_tracking.invalidate_for_logging();  // invalidate surface tracking alt, flight mode will set to true if used
#endif
    attitude_control->landed_gain_reduction(copter.ap.land_complete); // Adjust gains when landed to attenuate ground oscillation

    flightmode->run();
}

// exit_mode - high level call to organise cleanup as a flight mode is exited
void Copter::exit_mode(Mode *&old_flightmode,
                       Mode *&new_flightmode)
{
    // smooth throttle transition when switching from manual to automatic flight modes
    if (old_flightmode->has_manual_throttle() && !new_flightmode->has_manual_throttle() && motors->armed() && !ap.land_complete) {
        // this assumes all manual flight modes use get_pilot_desired_throttle to translate pilot input to output throttle
        set_accel_throttle_I_from_pilot_throttle();
    }

    // cancel any takeoffs in progress
    old_flightmode->takeoff_stop();

    // perform cleanup required for each flight mode
    old_flightmode->exit();

#if FRAME_CONFIG == HELI_FRAME
    // firmly reset the flybar passthrough to false when exiting acro mode.
    if (old_flightmode == &mode_acro) {
        attitude_control->use_flybar_passthrough(false, false);
        motors->set_acro_tail(false);
    }

    // if we are changing from a mode that did not use manual throttle,
    // stab col ramp value should be pre-loaded to the correct value to avoid a twitch
    // heli_stab_col_ramp should really only be active switching between Stabilize and Acro modes
    if (!old_flightmode->has_manual_throttle()){
        if (new_flightmode == &mode_stabilize){
            input_manager.set_stab_col_ramp(1.0);
        } else if (new_flightmode == &mode_acro){
            input_manager.set_stab_col_ramp(0.0);
        }
    }

    // Make sure inverted flight is disabled if not supported in the new mode
    if (!new_flightmode->allows_inverted()) {
        attitude_control->set_inverted_flight(false);
    }
#endif //HELI_FRAME
}

// notify_flight_mode - sets notify object based on current flight mode.  Only used for OreoLED notify device
void Copter::notify_flight_mode() {
    AP_Notify::flags.autopilot_mode = flightmode->is_autopilot();
    AP_Notify::flags.flight_mode = (uint8_t)flightmode->mode_number();
    notify.set_flight_mode_str(flightmode->name4());
}

// get_pilot_desired_angle - transform pilot's roll or pitch input into a desired lean angle
// returns desired angle in radians
void Mode::get_pilot_desired_lean_angles_rad(float &roll_out_rad, float &pitch_out_rad, float angle_max_rad, float angle_limit_rad) const
{
    // throttle failsafe check
    if (!rc().has_valid_input()) {
        roll_out_rad = 0.0;
        pitch_out_rad = 0.0;
        return;
    }

    //transform pilot's normalised roll or pitch stick input into a roll and pitch euler angle command
    rc_input_to_roll_pitch_rad(channel_roll->norm_input_dz(), channel_pitch->norm_input_dz(), angle_max_rad,  angle_limit_rad, roll_out_rad, pitch_out_rad);
}

// transform pilot's roll or pitch input into a desired velocity
Vector2f Mode::get_pilot_desired_velocity(float vel_max) const
{
    Vector2f vel;

    if (!rc().has_valid_input()) {
        return vel;
    }
    // fetch roll and pitch inputs
    float roll_out = channel_roll->norm_input_dz();
    float pitch_out = channel_pitch->norm_input_dz();

    // convert roll and pitch inputs into velocity in NE frame
    vel = Vector2f(-pitch_out, roll_out);
    if (vel.is_zero()) {
        return vel;
    }
    copter.rotate_body_frame_to_NE(vel.x, vel.y);

    // Transform square input range to circular output
    // vel_scalar is the vector to the edge of the +- 1.0 square in the direction of the current input
    Vector2f vel_scalar = vel / MAX(fabsf(vel.x), fabsf(vel.y));
    // We scale the output by the ratio of the distance to the square to the unit circle and multiply by vel_max
    vel *= vel_max / vel_scalar.length();
    return vel;
}

bool Mode::_TakeOff::triggered(const float target_climb_rate_cms) const
{
    if (!copter.ap.land_complete) {
        // can't take off if we're already flying
        return false;
    }
    if (target_climb_rate_cms <= 0.0f) {
        // can't takeoff unless we want to go up...
        return false;
    }

    if (copter.motors->get_spool_state() != AP_Motors::SpoolState::THROTTLE_UNLIMITED) {
        // hold aircraft on the ground until rotor speed runup has finished
        return false;
    }

    return true;
}

bool Mode::is_disarmed_or_landed() const
{
    if (!motors->armed() || !copter.ap.auto_armed || copter.ap.land_complete) {
        return true;
    }
    return false;
}

void Mode::zero_throttle_and_relax_ac(bool spool_up)
{
    if (spool_up) {
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);
    } else {
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
    }
    attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw_rad(0.0f, 0.0f, 0.0f);
    attitude_control->set_throttle_out(0.0f, false, copter.g.throttle_filt);
}

void Mode::zero_throttle_and_hold_attitude()
{
    // run attitude controller
    attitude_control->input_rate_bf_roll_pitch_yaw_rads(0.0f, 0.0f, 0.0f);
    attitude_control->set_throttle_out(0.0f, false, copter.g.throttle_filt);
}

// handle situations where the vehicle is on the ground waiting for takeoff
// force_throttle_unlimited should be true in cases where we want to keep the motors spooled up
// (instead of spooling down to ground idle).  This is required for tradheli's in Guided and Auto
// where we always want the motor spooled up in Guided or Auto mode.  Tradheli's main rotor stops 
// when spooled down to ground idle.
// ultimately it forces the motor interlock to be obeyed in auto and guided modes when on the ground.
void Mode::make_safe_ground_handling(bool force_throttle_unlimited)
{
    if (force_throttle_unlimited) {
        // keep rotors turning 
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);
    } else {
        // spool down to ground idle
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
    }

    // aircraft is landed, integrator terms must be reset regardless of spool state
    attitude_control->reset_rate_controller_I_terms_smoothly();
 
    switch (motors->get_spool_state()) {
    case AP_Motors::SpoolState::SHUT_DOWN:
    case AP_Motors::SpoolState::GROUND_IDLE:
        // reset yaw targets and rates during idle states
        attitude_control->reset_yaw_target_and_rate();
        break;
    case AP_Motors::SpoolState::SPOOLING_UP:
    case AP_Motors::SpoolState::THROTTLE_UNLIMITED:
    case AP_Motors::SpoolState::SPOOLING_DOWN:
        // while transitioning though active states continue to operate normally
        break;
    }

    pos_control->relax_velocity_controller_NE();
    pos_control->update_NE_controller();
    pos_control->relax_U_controller(0.0f);   // forces throttle output to decay to zero
    pos_control->update_U_controller();
    // we may need to move this out
    attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw_rad(0.0f, 0.0f, 0.0f);
}

/*
  get a height above ground estimate for landing
 */
int32_t Mode::get_alt_above_ground_cm(void) const
{
    int32_t alt_above_ground_cm;
    if (copter.get_rangefinder_height_interpolated_cm(alt_above_ground_cm)) {
        return alt_above_ground_cm;
    }
    if (!pos_control->is_active_NE()) {
        return copter.current_loc.alt;
    }
    if (copter.current_loc.get_alt_cm(Location::AltFrame::ABOVE_TERRAIN, alt_above_ground_cm)) {
        return alt_above_ground_cm;
    }

    // Assume the Earth is flat:
    return copter.current_loc.alt;
}

void Mode::land_run_vertical_control(bool pause_descent)
{
    float cmb_rate = 0;
    bool ignore_descent_limit = false;
    if (!pause_descent) {

        // do not ignore limits until we have slowed down for landing
        ignore_descent_limit = (MAX(g2.land_alt_low,100) > get_alt_above_ground_cm()) || copter.ap.land_complete_maybe;

        float max_land_descent_velocity;
        if (g.land_speed_high > 0) {
            max_land_descent_velocity = -g.land_speed_high;
        } else {
            max_land_descent_velocity = pos_control->get_max_speed_down_cms();
        }

        // Don't speed up for landing.
        max_land_descent_velocity = MIN(max_land_descent_velocity, -abs(g.land_speed));

        // Compute a vertical velocity demand such that the vehicle approaches g2.land_alt_low. Without the below constraint, this would cause the vehicle to hover at g2.land_alt_low.
        cmb_rate = sqrt_controller(MAX(g2.land_alt_low,100)-get_alt_above_ground_cm(), pos_control->get_pos_U_p().kP(), pos_control->get_max_accel_U_cmss(), G_Dt);

        // Constrain the demanded vertical velocity so that it is between the configured maximum descent speed and the configured minimum descent speed.
        cmb_rate = constrain_float(cmb_rate, max_land_descent_velocity, -abs(g.land_speed));

#if AC_PRECLAND_ENABLED
        const bool navigating = pos_control->is_active_NE();
        bool doing_precision_landing = !copter.ap.land_repo_active && copter.precland.target_acquired() && navigating;

        if (doing_precision_landing) {
            // prec landing is active
            Vector2f target_pos;
            float target_error_cm = 0.0f;
            if (copter.precland.get_target_position_cm(target_pos)) {
                const Vector2f current_pos = pos_control->get_pos_estimate_NEU_cm().xy().tofloat();
                // target is this many cm away from the vehicle
                target_error_cm = (target_pos - current_pos).length();
            }
            // check if we should descend or not
            const float max_horiz_pos_error_cm = copter.precland.get_max_xy_error_before_descending_cm();
            Vector3f target_pos_meas;
            copter.precland.get_target_position_measurement_cm(target_pos_meas);
            if (target_error_cm > max_horiz_pos_error_cm && !is_zero(max_horiz_pos_error_cm)) {
                // doing precland but too far away from the obstacle
                // do not descend
                cmb_rate = 0.0f;
            } else if (target_pos_meas.z > 35.0f && target_pos_meas.z < 200.0f && !copter.precland.do_fast_descend()) {
                // very close to the ground and doing prec land, lets slow down to make sure we land on target
                // compute desired descent velocity
                const float precland_acceptable_error_cm = 15.0f;
                const float precland_min_descent_speed_cms = 10.0f;
                const float max_descent_speed_cms = abs(g.land_speed)*0.5f;
                const float land_slowdown = MAX(0.0f, target_error_cm*(max_descent_speed_cms/precland_acceptable_error_cm));
                cmb_rate = MIN(-precland_min_descent_speed_cms, -max_descent_speed_cms+land_slowdown);
            }
        }
#endif
    }

    // update altitude target and call position controller
    pos_control->land_at_climb_rate_cm(cmb_rate, ignore_descent_limit);
    pos_control->update_U_controller();
}

void Mode::land_run_horizontal_control()
{
    Vector2f vel_correction;

    // relax loiter target if we might be landed
    if (copter.ap.land_complete_maybe) {
        pos_control->soften_for_landing_NE();
    }

    // process pilot inputs
    if (rc().has_valid_input()) {
        if ((g.throttle_behavior & THR_BEHAVE_HIGH_THROTTLE_CANCELS_LAND) != 0 && copter.rc_throttle_control_in_filter.get() > LAND_CANCEL_TRIGGER_THR){
            LOGGER_WRITE_EVENT(LogEvent::LAND_CANCELLED_BY_PILOT);
            // exit land if throttle is high
            if (!set_mode(Mode::Number::LOITER, ModeReason::THROTTLE_LAND_ESCAPE)) {
                set_mode(Mode::Number::ALT_HOLD, ModeReason::THROTTLE_LAND_ESCAPE);
            }
        }

        if (g.land_repositioning) {
            // apply SIMPLE mode transform to pilot inputs
            update_simple_mode();

            // convert pilot input to reposition velocity
            // use half maximum acceleration as the maximum velocity to ensure aircraft will
            // stop from full reposition speed in less than 1 second.
            const float max_pilot_vel = wp_nav->get_wp_acceleration_cmss() * 0.5;
            vel_correction = get_pilot_desired_velocity(max_pilot_vel);

            // record if pilot has overridden roll or pitch
            if (!vel_correction.is_zero()) {
                if (!copter.ap.land_repo_active) {
                    LOGGER_WRITE_EVENT(LogEvent::LAND_REPO_ACTIVE);
                }
                copter.ap.land_repo_active = true;
#if AC_PRECLAND_ENABLED
            } else {
                // no override right now, check if we should allow precland
                if (copter.precland.allow_precland_after_reposition()) {
                    copter.ap.land_repo_active = false;
                }
#endif
            }
        }
    }

    // this variable will be updated if prec land target is in sight and pilot isn't trying to reposition the vehicle
    copter.ap.prec_land_active = false;
#if AC_PRECLAND_ENABLED
    copter.ap.prec_land_active = !copter.ap.land_repo_active && copter.precland.target_acquired();
    // run precision landing
    if (copter.ap.prec_land_active) {
        Vector2f target_pos, target_vel;
        if (!copter.precland.get_target_position_cm(target_pos)) {
            target_pos = pos_control->get_pos_estimate_NEU_cm().xy().tofloat();
        }
         // get the velocity of the target
        copter.precland.get_target_velocity_cms(pos_control->get_vel_estimate_NEU_cms().xy(), target_vel);

        Vector2f zero;
        Vector2p landing_pos = target_pos.topostype();
        // target vel will remain zero if landing target is stationary
        pos_control->input_pos_vel_accel_NE_cm(landing_pos, target_vel, zero);
    }
#endif

    if (!copter.ap.prec_land_active) {
        Vector2f accel;
        pos_control->input_vel_accel_NE_cm(vel_correction, accel);
    }

    // run pos controller
    pos_control->update_NE_controller();
    Vector3f thrust_vector = pos_control->get_thrust_vector();

    // call attitude controller
    attitude_control->input_thrust_vector_heading(thrust_vector, auto_yaw.get_heading());

}

// run normal or precision landing (if enabled)
// pause_descent is true if vehicle should not descend
void Mode::land_run_normal_or_precland(bool pause_descent)
{
#if AC_PRECLAND_ENABLED
    if (pause_descent || !copter.precland.enabled()) {
        // we don't want to start descending immediately or prec land is disabled
        // in both cases just run simple land controllers
        land_run_horiz_and_vert_control(pause_descent);
    } else {
        // prec land is enabled and we have not paused descent
        // the state machine takes care of the entire prec landing procedure
        precland_run();
    }
#else
    land_run_horiz_and_vert_control(pause_descent);
#endif
}

#if AC_PRECLAND_ENABLED
// Go towards a position commanded by prec land state machine in order to retry landing
// The passed in location is expected to be NED and in m
void Mode::precland_retry_position(const Vector3f &retry_pos)
{
    if (rc().has_valid_input()) {
        if ((g.throttle_behavior & THR_BEHAVE_HIGH_THROTTLE_CANCELS_LAND) != 0 && copter.rc_throttle_control_in_filter.get() > LAND_CANCEL_TRIGGER_THR){
            LOGGER_WRITE_EVENT(LogEvent::LAND_CANCELLED_BY_PILOT);
            // exit land if throttle is high
            if (!set_mode(Mode::Number::LOITER, ModeReason::THROTTLE_LAND_ESCAPE)) {
                set_mode(Mode::Number::ALT_HOLD, ModeReason::THROTTLE_LAND_ESCAPE);
            }
        }

        // allow user to take control during repositioning. Note: copied from land_run_horizontal_control()
        // To-Do: this code exists at several different places in slightly different forms and that should be fixed
        if (g.land_repositioning) {
            float target_roll_rad = 0.0f;
            float target_pitch_rad = 0.0f;
            // convert pilot input to lean angles
            get_pilot_desired_lean_angles_rad(target_roll_rad, target_pitch_rad, loiter_nav->get_angle_max_rad(), attitude_control->get_althold_lean_angle_max_rad());

            // record if pilot has overridden roll or pitch
            if (!is_zero(target_roll_rad) || !is_zero(target_pitch_rad)) {
                if (!copter.ap.land_repo_active) {
                    LOGGER_WRITE_EVENT(LogEvent::LAND_REPO_ACTIVE);
                }
                // this flag will be checked by prec land state machine later and any further landing retires will be cancelled
                copter.ap.land_repo_active = true;
            }
        }
    }

    Vector3p retry_pos_NEU{retry_pos.x, retry_pos.y, retry_pos.z * -1.0f};
    // pos controller expects input in NEU cm's
    retry_pos_NEU = retry_pos_NEU * 100.0f;
    pos_control->input_pos_NEU_cm(retry_pos_NEU, 0.0f, 1000.0f);

    // run position controllers
    pos_control->update_NE_controller();
    pos_control->update_U_controller();

    // call attitude controller
    attitude_control->input_thrust_vector_heading(pos_control->get_thrust_vector(), auto_yaw.get_heading());
}

// Run precland statemachine. This function should be called from any mode that wants to do precision landing.
// This handles everything from prec landing, to prec landing failures, to retries and failsafe measures
void Mode::precland_run()
{
    // if user is taking control, we will not run the statemachine, and simply land (may or may not be on target)
    if (!copter.ap.land_repo_active) {
        // This will get updated later to a retry pos if needed
        Vector3f retry_pos;

        switch (copter.precland_statemachine.update(retry_pos)) {
        case AC_PrecLand_StateMachine::Status::RETRYING:
            // we want to retry landing by going to another position
            precland_retry_position(retry_pos);
            break;

        case AC_PrecLand_StateMachine::Status::FAILSAFE: {
            // we have hit a failsafe. Failsafe can only mean two things, we either want to stop permanently till user takes over or land
            switch (copter.precland_statemachine.get_failsafe_actions()) {
            case AC_PrecLand_StateMachine::FailSafeAction::DESCEND:
                // descend normally, prec land target is definitely not in sight
                land_run_horiz_and_vert_control();
                break;
            case AC_PrecLand_StateMachine::FailSafeAction::HOLD_POS:
                // sending "true" in this argument will stop the descend
                land_run_horiz_and_vert_control(true);
                break;
            }
            break;
        }
        case AC_PrecLand_StateMachine::Status::ERROR:
            // should never happen, is certainly a bug. Report then descend
            INTERNAL_ERROR(AP_InternalError::error_t::flow_of_control);
            FALLTHROUGH;
        case AC_PrecLand_StateMachine::Status::DESCEND:
            // run land controller. This will descend towards the target if prec land target is in sight
            // else it will just descend vertically
            land_run_horiz_and_vert_control();
            break;
        }
    } else {
        // just land, since user has taken over controls, it does not make sense to run any retries or failsafe measures
        land_run_horiz_and_vert_control();
    }
}
#endif

float Mode::throttle_hover() const
{
    return motors->get_throttle_hover();
}

// transform pilot's manual throttle input to make hover throttle mid stick
// used only for manual throttle modes
// thr_mid should be in the range 0 to 1
// returns throttle output 0 to 1
float Mode::get_pilot_desired_throttle() const
{
    int16_t mid_stick = copter.get_throttle_mid();
    // protect against unlikely divide by zero
    if (mid_stick <= 0) {
        mid_stick = 500;
    }

    int16_t throttle_control = channel_throttle->get_control_in();
    // ensure reasonable throttle values
    throttle_control = constrain_int16(throttle_control,0,1000);

    // calculate normalised throttle input
    float throttle_in;
    if (throttle_control < mid_stick) {
        throttle_in = ((float)throttle_control)*0.5f/(float)mid_stick;
    } else {
        throttle_in = 0.5f + ((float)(throttle_control-mid_stick)) * 0.5f / (float)(1000-mid_stick);
    }

    const float thr_mid = throttle_hover();
    const float expo = constrain_float(-(thr_mid-0.5f)/0.375f, -0.5f, 1.0f);
    // calculate the output throttle using the given expo function
    float throttle_out = throttle_in*(1.0f-expo) + expo*throttle_in*throttle_in*throttle_in;
    return throttle_out;
}

float Mode::get_avoidance_adjusted_climbrate_cms(float target_rate_cms)
{
#if AP_AVOIDANCE_ENABLED
    AP::ac_avoid()->adjust_velocity_z(pos_control->get_pos_U_p().kP(), pos_control->get_max_accel_U_cmss(), target_rate_cms, G_Dt);
    return target_rate_cms;
#else
    return target_rate_cms;
#endif
}

// send output to the motors, can be overridden by subclasses
void Mode::output_to_motors()
{
    motors->output();
}

Mode::AltHoldModeState Mode::get_alt_hold_state(float target_climb_rate_cms)
{
    // Alt Hold State Machine Determination
    if (!motors->armed()) {
        // the aircraft should moved to a shut down state
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::SHUT_DOWN);

        // transition through states as aircraft spools down
        switch (motors->get_spool_state()) {

        case AP_Motors::SpoolState::SHUT_DOWN:
            return AltHoldModeState::MotorStopped;

        case AP_Motors::SpoolState::GROUND_IDLE:
            return AltHoldModeState::Landed_Ground_Idle;

        default:
            return AltHoldModeState::Landed_Pre_Takeoff;
        }

    } else if (takeoff.running() || takeoff.triggered(target_climb_rate_cms)) {
        // the aircraft is currently landed or taking off, asking for a positive climb rate and in THROTTLE_UNLIMITED
        // the aircraft should progress through the take off procedure
        return AltHoldModeState::Takeoff;

    } else if (!copter.ap.auto_armed || copter.ap.land_complete) {
        // the aircraft is armed and landed
        if (target_climb_rate_cms < 0.0f && !copter.ap.using_interlock) {
            // the aircraft should move to a ground idle state
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);

        } else {
            // the aircraft should prepare for imminent take off
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);
        }

        if (motors->get_spool_state() == AP_Motors::SpoolState::GROUND_IDLE) {
            // the aircraft is waiting in ground idle
            return AltHoldModeState::Landed_Ground_Idle;

        } else {
            // the aircraft can leave the ground at any time
            return AltHoldModeState::Landed_Pre_Takeoff;
        }

    } else {
        // the aircraft is in a flying state
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);
        return AltHoldModeState::Flying;
    }
}

// transform pilot's yaw input into a desired yaw rate
// returns desired yaw rate in centi-degrees per second
float Mode::get_pilot_desired_yaw_rate_rads() const
{
    if (!rc().has_valid_input()) {
        return 0.0f;
    }

    // Get yaw input
    const float yaw_in = channel_yaw->norm_input_dz();

    // convert pilot input to the desired yaw rate
    return radians(g2.command_model_pilot_y.get_rate()) * input_expo(yaw_in, g2.command_model_pilot_y.get_expo());
}

// pass-through functions to reduce code churn on conversion;
// these are candidates for moving into the Mode base
// class.
float Mode::get_pilot_desired_climb_rate()
{
    return copter.get_pilot_desired_climb_rate();
}

float Mode::get_non_takeoff_throttle()
{
    return copter.get_non_takeoff_throttle();
}

void Mode::update_simple_mode(void) {
    copter.update_simple_mode();
}

bool Mode::set_mode(Mode::Number mode, ModeReason reason)
{
    return copter.set_mode(mode, reason);
}

void Mode::set_land_complete(bool b)
{
    return copter.set_land_complete(b);
}

GCS_Copter &Mode::gcs()
{
    return copter.gcs();
}

uint16_t Mode::get_pilot_speed_dn()
{
    return copter.get_pilot_speed_dn();
}

// Return stopping point as a location with above origin alt frame
Location Mode::get_stopping_point() const
{
    Vector3p stopping_point_neu_cm;
    copter.pos_control->get_stopping_point_NE_cm(stopping_point_neu_cm.xy());
    copter.pos_control->get_stopping_point_U_cm(stopping_point_neu_cm.z);
    return Location { stopping_point_neu_cm.tofloat(), Location::AltFrame::ABOVE_ORIGIN };
}
