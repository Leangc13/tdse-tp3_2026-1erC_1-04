/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"

#include "task_actuator_attribute.h"
#include "task_actuator_interface.h"
#include "task_display_attribute.h"
#include "task_display_interface.h"
#include "task_system_attribute.h"
#include "task_system_interface.h"

/********************** macros and definitions *******************************/
#define DEL_SYS_MIN		0ul

#define SYSTEM_DTA_QTY	1ul

/********************** internal data declaration ****************************/
task_system_dta_t task_system_dta_list[SYSTEM_DTA_QTY];

/********************** internal functions declaration ***********************/
void task_system_statechart(void);

/********************** internal data definition *****************************/
const char *p_task_system		= "Task System (System Statechart)";
const char *p_task_system_		= "Non-Blocking Code";
const char *p_task_system__		= "(Update by Time Code, period = 1mS)";

/********************** external functions definition ************************/
void task_system_init(void *parameters)
{
	task_system_dta_t *p_task_system_dta;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", GET_NAME(task_system_init), HAL_GetTick());
	LOGGER_INFO("   %s is a %s", GET_NAME(task_system), p_task_system);
	LOGGER_INFO("   %s is a %s", GET_NAME(task_system), p_task_system_);
	LOGGER_INFO("   %s is a %s", GET_NAME(task_system), p_task_system__);

	init_event_task_system();

	p_task_system_dta = &task_system_dta_list[0];

	p_task_system_dta->state		= ST_SYS_MAIN;
	p_task_system_dta->event		= EV_SYS_IDLE;
	p_task_system_dta->flag			= false;
	p_task_system_dta->selected_motor	= 1;
	p_task_system_dta->mx_power		= false;
	p_task_system_dta->mx_speed		= 0;
	p_task_system_dta->mx_spin		= 'R';
	p_task_system_dta->aux_speed		= 0;

	/* Display initial screen */
	put_event_task_display(0, 0, "system_setup_menu");
	put_event_task_display(0, 1, "   <Main>       ");
}

void task_system_update(void *parameters)
{
	/* Run Task Statechart */
	task_system_statechart();
}

/*
 * Helper: build a display string for edit_speed showing current aux value.
 * put_event_task_display expects a const char*, so we use a static buffer.
 */
static void display_speed(task_system_dta_t *p_dta)
{
	static char buf[17];
	/* Format: "   speed: X     " (16 chars) */
	buf[0]  = ' '; buf[1]  = ' '; buf[2]  = ' ';
	buf[3]  = 's'; buf[4]  = 'p'; buf[5]  = 'e';
	buf[6]  = 'e'; buf[7]  = 'd'; buf[8]  = ':';
	buf[9]  = ' ';
	buf[10] = (char)('0' + (p_dta->aux_speed % 10));
	buf[11] = ' '; buf[12] = ' '; buf[13] = ' ';
	buf[14] = ' '; buf[15] = ' '; buf[16] = '\0';
	put_event_task_display(0, 1, buf);
}

void task_system_statechart(void)
{
	task_system_dta_t *p_task_system_dta;

	p_task_system_dta = &task_system_dta_list[0];

	/* Consume one event from the queue.
	 * EV_SYS_IDLE (generated on button-release) is discarded — it has no
	 * semantic meaning in the system statechart. */
	if (true == any_event_task_system())
	{
		p_task_system_dta->event = get_event_task_system();
		if (EV_SYS_IDLE != p_task_system_dta->event)
		{
			p_task_system_dta->flag = true;
		}
	}

	/* Only act when a real event is pending */
	if (false == p_task_system_dta->flag)
	{
		return;
	}

	/* Consume the flag before the switch so every path is safe */
	p_task_system_dta->flag = false;

	switch (p_task_system_dta->state)
	{
		/* ------------------------------------------------------------------ */
		/*  Main                                                               */
		/* ------------------------------------------------------------------ */
		case ST_SYS_MAIN:

			if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_SELECT_MOTOR_1;

				put_event_task_display(0, 0, "  Menu_1 <name> ");
				put_event_task_display(0, 1, " >Motor_1       ");
			}
			/* NEXT and ESCAPE have no effect in Main */

			break;

		/* ------------------------------------------------------------------ */
		/*  Menu_1 – Select Motor                                              */
		/* ------------------------------------------------------------------ */
		case ST_SYS_SELECT_MOTOR_1:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_SELECT_MOTOR_2;

				put_event_task_display(0, 0, "  Menu_1 <name> ");
				put_event_task_display(0, 1, " >Motor_2       ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->selected_motor = 1;
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				put_event_task_display(0, 0, " M1 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_MAIN;

				put_event_task_display(0, 0, "system_setup_menu");
				put_event_task_display(0, 1, "   <Main>       ");
			}

			break;

		case ST_SYS_SELECT_MOTOR_2:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_SELECT_MOTOR_1;

				put_event_task_display(0, 0, "  Menu_1 <name> ");
				put_event_task_display(0, 1, " >Motor_1       ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->selected_motor = 2;
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_MAIN;

				put_event_task_display(0, 0, "system_setup_menu");
				put_event_task_display(0, 1, "   <Main>       ");
			}

			break;

		/* ------------------------------------------------------------------ */
		/*  Menu_2 – Select Parameter                                          */
		/* ------------------------------------------------------------------ */
		case ST_SYS_PARAM_POWER:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_SPEED;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Speed         ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_EDIT_POWER_ON;

				put_event_task_display(0, 0, "  edit_power    ");
				put_event_task_display(0, 1, "   >ON          ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				/* Return to the motor that was selected */
				if (1 == p_task_system_dta->selected_motor)
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_1;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_1       ");
				}
				else
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_2;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_2       ");
				}
			}

			break;

		case ST_SYS_PARAM_SPEED:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_SPIN;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Spin          ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->aux_speed = p_task_system_dta->mx_speed;
				p_task_system_dta->state = ST_SYS_EDIT_SPEED;

				put_event_task_display(0, 0, "  edit_speed    ");
				display_speed(p_task_system_dta);
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				if (1 == p_task_system_dta->selected_motor)
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_1;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_1       ");
				}
				else
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_2;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_2       ");
				}
			}

			break;

		case ST_SYS_PARAM_SPIN:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_spin = 'R';
				p_task_system_dta->state = ST_SYS_EDIT_SPIN_RIGHT;

				put_event_task_display(0, 0, "  edit_spin     ");
				put_event_task_display(0, 1, "   >RIGHT       ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				if (1 == p_task_system_dta->selected_motor)
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_1;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_1       ");
				}
				else
				{
					p_task_system_dta->state = ST_SYS_SELECT_MOTOR_2;
					put_event_task_display(0, 0, "  Menu_1 <name> ");
					put_event_task_display(0, 1, " >Motor_2       ");
				}
			}

			break;

		/* ------------------------------------------------------------------ */
		/*  Menu_3 – edit_power                                               */
		/* ------------------------------------------------------------------ */
		case ST_SYS_EDIT_POWER_ON:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_EDIT_POWER_OFF;

				put_event_task_display(0, 0, "  edit_power    ");
				put_event_task_display(0, 1, "   >OFF         ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_power = true;
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}

			break;

		case ST_SYS_EDIT_POWER_OFF:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_EDIT_POWER_ON;

				put_event_task_display(0, 0, "  edit_power    ");
				put_event_task_display(0, 1, "   >ON          ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_power = false;
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_POWER;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Power         ");
			}

			break;

		/* ------------------------------------------------------------------ */
		/*  Menu_3 – edit_speed                                               */
		/* ------------------------------------------------------------------ */
		case ST_SYS_EDIT_SPEED:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				/* aux = (aux + 1) % 10  — wraps 0..9 */
				p_task_system_dta->aux_speed = (p_task_system_dta->aux_speed + 1) % 10;

				put_event_task_display(0, 0, "  edit_speed    ");
				display_speed(p_task_system_dta);
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_speed = p_task_system_dta->aux_speed;
				p_task_system_dta->state = ST_SYS_PARAM_SPEED;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Speed         ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_SPEED;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Speed         ");
			}

			break;

		/* ------------------------------------------------------------------ */
		/*  Menu_3 – edit_spin                                                 */
		/* ------------------------------------------------------------------ */
		case ST_SYS_EDIT_SPIN_RIGHT:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_EDIT_SPIN_LEFT;

				put_event_task_display(0, 0, "  edit_spin     ");
				put_event_task_display(0, 1, "   >LEFT        ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_spin = 'R';
				p_task_system_dta->state = ST_SYS_PARAM_SPIN;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Spin          ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_SPIN;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Spin          ");
			}

			break;

		case ST_SYS_EDIT_SPIN_LEFT:

			if (EV_SYS_NEXT == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_EDIT_SPIN_RIGHT;

				put_event_task_display(0, 0, "  edit_spin     ");
				put_event_task_display(0, 1, "   >RIGHT       ");
			}
			else if (EV_SYS_ENTER == p_task_system_dta->event)
			{
				p_task_system_dta->mx_spin = 'L';
				p_task_system_dta->state = ST_SYS_PARAM_SPIN;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Spin          ");
			}
			else if (EV_SYS_ESCAPE == p_task_system_dta->event)
			{
				p_task_system_dta->state = ST_SYS_PARAM_SPIN;

				if (1 == p_task_system_dta->selected_motor)
					put_event_task_display(0, 0, " M1 Menu_2      ");
				else
					put_event_task_display(0, 0, " M2 Menu_2      ");
				put_event_task_display(0, 1, " >Spin          ");
			}

			break;

		/* ------------------------------------------------------------------ */
		/*  Default – recovery                                                 */
		/* ------------------------------------------------------------------ */
		default:

			p_task_system_dta->tick		= DEL_SYS_MIN;
			p_task_system_dta->state	= ST_SYS_MAIN;
			p_task_system_dta->event	= EV_SYS_IDLE;
			p_task_system_dta->flag		= false;

			break;
	}
}

/********************** end of file ******************************************/
