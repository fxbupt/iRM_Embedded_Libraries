/**************************************************************************
 *  Copyright (C) 2018 
 *  Illini RoboMaster @ University of Illinois at Urbana-Champaign.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "gimbal.h"
#include "imu_onboard.h"
#include "motor.h"
#include "utils.h"

#ifdef INFANTRY1

void gimbal_init(gimbal_t *my_gimbal) {
    /* Init Yaw */
    motor_t *yaw;
    my_gimbal->yaw_middle   = INIT_MIDDLE_YAW;
    my_gimbal->yaw_ang      = 0;
#if defined(INFANTRY1) || defined(INFANTRY2) || defined(INFANTRY3)
    yaw = can_motor_init(NULL, 0x209, CAN1_ID, M6623);
    my_gimbal->yaw = pid_init(NULL, MANUAL_ERR_INPUT, yaw, 5200, 7700, 8000, 0, 0, 12, 0, 64, 4800, 0);
#elif defined(ENGINEERING)
    yaw = can_motor_init(NULL, 0x209, CAN1_ID, M6623);
    my_gimbal->yaw = pid_init(NULL, MANUAL_ERR_INPUT, yaw, 0, 0, 0, 0, 0, 4.2, 0, 35, 2500, 0);
#elif defined(HERO)
    yaw = can_motor_init(NULL, 0x209, CAN1_ID, M6623);
    my_gimbal->yaw = pid_init(NULL, MANUAL_ERR_INPUT, yaw, 0, 0, 0, 0, 0, 5.5, 0, 40, 4800, 0);
#endif
    /* Init Pitch*/
    motor_t *pitch;
    my_gimbal->pitch_ang = INIT_MIDDLE_PITCH;
#ifdef ENGINEERING
    pitch = can_motor_init(NULL, 0x205, CAN1_ID, M3510);
    my_gimbal->pitch = pid_init(NULL, GIMBAL_MAN_SHOOT, pitch, PITCH_LOW_LIMIT, PITCH_HIGH_LIMIT, 0, 0, 0, 6, 0, 10, 8000, 0);
#elif defined(INFANTRY1) || defined(INFANTRY2) || defined(INFANTRY3)
    pitch = can_motor_init(NULL, 0x20A, CAN1_ID, M6623);
    my_gimbal->pitch = pid_init(NULL, GIMBAL_MAN_SHOOT, pitch, PITCH_LOW_LIMIT, PITCH_HIGH_LIMIT, 10000, 0, 0, 6, 0.13, 18, 2500, 0);
#elif defined(HERO)
    pitch = can_motor_init(NULL, 0x20A, CAN1_ID, M6623);
    my_gimbal->pitch = pid_init(NULL, GIMBAL_MAN_SHOOT, pitch, PITCH_LOW_LIMIT, PITCH_HIGH_LIMIT, 6000, 0, 0, 6, 0.14, 20, 4000, 0);
#endif
    /* Init Camera Pitch */
    // not implemented yet
}

void gimbal_kill(gimbal_t *my_gimbal) {
    my_gimbal->pitch->motor->out = 0;
    my_gimbal->yaw->motor->out = 0;
    
    while (1) {
#if defined(INFANTRY1) || defined(INFANTRY2) || defined(INFANTRY3) || defined(HERO)
        set_can_motor_output(my_gimbal->yaw->motor, my_gimbal->pitch->motor, NULL, NULL);
#else
        set_can_motor_output(my_gimbal->yaw->motor, NULL, NULL, NULL);
        set_can_motor_output(my_gimbal->pitch->motor, NULL, NULL, NULL);
#endif
        osDelay(1000);
    }
}

void gimbal_mouse_move(gimbal_t *my_gimbal, dbus_t *rc, int32_t observed_abs_yaw) {
    my_gimbal->yaw_ang -= rc->mouse.x * 0.4;
    my_gimbal->pitch_ang -= rc->mouse.y * 0.4;
    fclip_to_range(&my_gimbal->pitch_ang, PITCH_LOW_LIMIT, PITCH_HIGH_LIMIT);
    my_gimbal->yaw->motor->out = pid_calc(my_gimbal->yaw, (int32_t)(my_gimbal->yaw_ang) - observed_abs_yaw);
    my_gimbal->pitch->motor->out = pid_calc(my_gimbal->pitch, (int32_t)(my_gimbal->pitch_ang));
}

void gimbal_remote_move(gimbal_t *my_gimbal, dbus_t *rc, int32_t observed_abs_yaw) {
    my_gimbal->yaw_ang -= rc->ch2 * 0.1;
    my_gimbal->pitch_ang += rc->ch3 * 0.1;
    fclip_to_range(&my_gimbal->pitch_ang, PITCH_LOW_LIMIT, PITCH_HIGH_LIMIT);
    my_gimbal->yaw->motor->out = pid_calc(my_gimbal->yaw, (int32_t)my_gimbal->yaw_ang - observed_abs_yaw);
    my_gimbal->pitch->motor->out = pid_calc(my_gimbal->pitch, my_gimbal->pitch_ang);
}

void gimbal_update(gimbal_t *my_gimbal) {
    //get_motor_data(my_gimbal->pitch->motor);
    get_motor_data(my_gimbal->yaw->motor);
    //get_motor_data(my_gimbal->camera_pitch->motor);
}

void yaw_ramp_ctl(gimbal_t *my_gimbal, int32_t delta_ang, uint16_t step_size) {
    int8_t direction = sign(delta_ang);
    int32_t observed_abs_yaw = (int32_t)(imuBoard.angle[YAW] * DEG_2_MOTOR);
    int32_t destination_ang = observed_abs_yaw + delta_ang;

    size_t i;

    for (i = 0; i < abs(delta_ang); i += step_size) {
        observed_abs_yaw = (int32_t)(imuBoard.angle[YAW] * DEG_2_MOTOR);
        my_gimbal->yaw_ang += direction * step_size;
        my_gimbal->yaw->motor->out = pid_calc(my_gimbal->yaw, (int32_t)(my_gimbal->yaw_ang) - observed_abs_yaw);
        my_gimbal->pitch->motor->out = pid_calc(my_gimbal->pitch, (int32_t)my_gimbal->pitch_ang);
        run_gimbal(my_gimbal);
        osDelay(20);
    }

    /* delay 200 ms and hold for the destination angle */
    my_gimbal->yaw_ang = destination_ang;
    for (i = 0; i < 20; i++) {
        observed_abs_yaw = (int32_t)(imuBoard.angle[YAW] * DEG_2_MOTOR);
        my_gimbal->yaw->motor->out = pid_calc(my_gimbal->yaw, (int32_t)(my_gimbal->yaw_ang) - observed_abs_yaw);
        my_gimbal->pitch->motor->out = pid_calc(my_gimbal->pitch, (int32_t)my_gimbal->pitch_ang);
        run_gimbal(my_gimbal);
        osDelay(20);
    }
}

void gimbal_set_yaw_angle(gimbal_t *my_gimbal, int32_t yaw_ang) {
    my_gimbal->yaw->mode = GIMBAL_MAN_SHOOT;
    my_gimbal->yaw->motor->out = pid_calc(my_gimbal->yaw, yaw_ang);
    my_gimbal->pitch->motor->out = pid_calc(my_gimbal->pitch, my_gimbal->pitch_ang);
    my_gimbal->yaw->mode = MANUAL_ERR_INPUT;
    run_gimbal(my_gimbal);
}

void run_gimbal(gimbal_t *my_gimbal) {
    /* Run Pitch */
#if defined(INFANTRY1) || defined(INFANTRY2) || defined(INFANTRY3) || defined(HERO)
    set_can_motor_output(my_gimbal->yaw->motor, my_gimbal->pitch->motor, NULL, NULL);
#else
    set_can_motor_output(my_gimbal->yaw->motor, NULL, NULL, NULL);
    set_can_motor_output(my_gimbal->pitch->motor, NULL, NULL, NULL);
#endif
    //set_can_motor_output(my_gimbal->camera_pitch->motor, NULL, NULL, NULL);
}

#endif
