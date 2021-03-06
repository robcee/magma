/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the Apache License, Version 2.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*****************************************************************************

  Source      mme_app_sgs_alert.c

  Version

  Date

  Product    MME app

  Subsystem  SGS (an interface between MME and MSC/VLR) message handling for
             non-eps alert procedure

  Author

  Description Handles non-eps procedure

*****************************************************************************/

#include "log.h"
#include "service303.h"
#include "mme_config.h"
#include "intertask_interface.h"
#include "mme_app_sgs_fsm.h"
#include "assertions.h"
#include "conversions.h"
#include "mme_app_defs.h"

static int _mme_app_send_sgsap_alert_reject(
  itti_sgsap_alert_request_t *const sgsap_alert_req_pP,
  SgsCause_t sgs_cause,
  uint64_t imsi64);

static int _mme_app_send_sgsap_alert_ack(
  itti_sgsap_alert_request_t *const sgsap_alert_req_pP,
  uint64_t imsi64);

/****************************************************************************
 **                                                                        **
 ** Name:    mme_app_handle_sgsap_alert_request()                          **
 **                                                                        **
 ** Description: Processes the SGSAP alert Request message                 **
 **      received from the SGS task                                        **
 **                                                                        **
 ** Inputs:  itti_sgsap_alert_request_t: SGSAP alert Request message       **
 **                                                                        **
 ** Outputs:                                                               **
 **      Return:    RETURNok, RETURNerror                                  **
 **                                                                        **
 ***************************************************************************/

int mme_app_handle_sgsap_alert_request(
  itti_sgsap_alert_request_t *const sgsap_alert_req_pP)
{
  uint64_t imsi64 = 0;
  struct ue_mm_context_s *ue_context_p = NULL;

  OAILOG_FUNC_IN(LOG_MME_APP);
  DevAssert(sgsap_alert_req_pP);

  IMSI_STRING_TO_IMSI64(sgsap_alert_req_pP->imsi, &imsi64);

  OAILOG_INFO(
    LOG_MME_APP,
    "Received SGS-ALERT REQUEST for IMSI " IMSI_64_FMT "\n",
    imsi64);
  if (
    (ue_context_p = mme_ue_context_exists_imsi(
       &mme_app_desc.mme_ue_contexts, imsi64)) == NULL) {
    OAILOG_ERROR(
      LOG_MME_APP,
      "SGS-ALERT REQUEST: Failed to find UE context for IMSI " IMSI_64_FMT "\n",
      imsi64);
    _mme_app_send_sgsap_alert_reject(
      sgsap_alert_req_pP, SGS_CAUSE_IMSI_UNKNOWN, imsi64);
    increment_counter("sgsap_alert_reject", 1, 1, "cause", "imsi_unknown");
    OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNerror);
  }
  if (ue_context_p->mm_state == UE_UNREGISTERED) {
    OAILOG_INFO(
      LOG_MME_APP,
      "SGS-ALERT REQUEST: UE is currently not attached to EPS service and "
      "send Alert Reject to MSC/VLR for UE:" IMSI_64_FMT " \n",
      imsi64);
    _mme_app_send_sgsap_alert_reject(
      sgsap_alert_req_pP, SGS_CAUSE_IMSI_DETACHED_FOR_EPS_SERVICE, imsi64);
    increment_counter(
      "sgsap_alert_reject", 1, 1, "cause", "ue_is_not_registered_to_eps");
    unlock_ue_contexts(ue_context_p);
    OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNerror);
  }
  if (ue_context_p->sgs_context == NULL) {
    OAILOG_INFO(
      LOG_MME_APP,
      "SGS context not created for IMSI and creating sgs context"
      "on reception of Alert Request over SGS" IMSI_64_FMT "\n",
      imsi64);
    //Create SGS context
    ue_context_p->sgs_context = calloc(1, sizeof(sgs_context_t));
    if (ue_context_p->sgs_context == NULL) {
      OAILOG_ERROR(
        LOG_MME_APP,
        "Cannot create SGS Context for UE ID %d ",
        ue_context_p->mme_ue_s1ap_id);
      unlock_ue_contexts(ue_context_p);
      OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNerror);
    }
    /*Initialize SGS context to default values*/
    ue_context_p->sgs_context->sgs_state = SGS_NULL;
    ue_context_p->sgs_context->vlr_reliable = false;
    ue_context_p->sgs_context->ts6_1_timer.id = SGS_TIMER_INACTIVE_ID;
    ue_context_p->sgs_context->ts6_1_timer.sec =
      mme_config.sgs_config.ts6_1_sec;
    ue_context_p->sgs_context->ts8_timer.id = SGS_TIMER_INACTIVE_ID;
    ue_context_p->sgs_context->ts8_timer.sec = mme_config.sgs_config.ts8_sec;
    ue_context_p->sgs_context->ts9_timer.id = SGS_TIMER_INACTIVE_ID;
    ue_context_p->sgs_context->ts9_timer.sec = mme_config.sgs_config.ts9_sec;
    ue_context_p->sgs_context->ts10_timer.id = SGS_TIMER_INACTIVE_ID;
    ue_context_p->sgs_context->ts10_timer.sec = mme_config.sgs_config.ts10_sec;
    ue_context_p->sgs_context->ts13_timer.id = SGS_TIMER_INACTIVE_ID;
    ue_context_p->sgs_context->ts13_timer.sec = mme_config.sgs_config.ts13_sec;
  }

  ue_context_p->sgs_context->neaf = SET_NEAF;

  /* send Alert Ack */
  _mme_app_send_sgsap_alert_ack(sgsap_alert_req_pP, imsi64);
  unlock_ue_contexts(ue_context_p);
  OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNok);
}

/**********************************************************************************
 **                                                                              **
 ** Name:    _mme_app_send_sgsap_alert_reject()                                   **
 ** Description   Build and send Alert reject                                    **
 ** Inputs:                                                                      **
 **          sgsap_alert_req_pP: Received Alert Request message                  **
 **          sgs_cause         : alert reject cause                              **
 **          imsi              : imsi                                            **
 ** Outputs:                                                                     **
 **          Return:    RETURNok, RETURNerror                                    **
 **
***********************************************************************************/
static int _mme_app_send_sgsap_alert_reject(
  itti_sgsap_alert_request_t *const sgsap_alert_req_pP,
  SgsCause_t sgs_cause,
  uint64_t imsi64)
{
  int rc = RETURNerror;
  MessageDef *message_p = NULL;
  itti_sgsap_alert_reject_t *sgsap_alert_reject_pP = NULL;
  OAILOG_FUNC_IN(LOG_MME_APP);

  message_p = itti_alloc_new_message(TASK_MME_APP, SGSAP_ALERT_REJECT);
  AssertFatal(message_p, "itti_alloc_new_message Failed");
  sgsap_alert_reject_pP = &message_p->ittiMsg.sgsap_alert_reject;
  memset((void *) sgsap_alert_reject_pP, 0, sizeof(itti_sgsap_alert_reject_t));

  memcpy(
    (void *) sgsap_alert_reject_pP->imsi,
    (const void *) sgsap_alert_req_pP->imsi,
    sgsap_alert_req_pP->imsi_length);
  sgsap_alert_reject_pP->imsi_length = sgsap_alert_req_pP->imsi_length;
  sgsap_alert_reject_pP->sgs_cause = sgs_cause;

  OAILOG_INFO(
    LOG_MME_APP,
    "Send SGSAP-Alert Reject for IMSI" IMSI_64_FMT " with sgs-cause :%d \n",
    imsi64,
    (int) sgs_cause);
  rc = itti_send_msg_to_task(TASK_SGS, INSTANCE_DEFAULT, message_p);
  OAILOG_FUNC_RETURN(LOG_MME_APP, rc);
}

/**********************************************************************************
 **                                                                              **
 ** Name:    _mme_app_send_sgsap_alert_ack()                                      **
 ** Description   Build and send Alert ack                                       **
 ** Inputs:                                                                      **
 **          sgsap_alert_req_pP: Received Alert ack message                      **
 **          imsi64        : imsi                                                **
 ** Outputs:                                                                     **
 **          Return:    RETURNok, RETURNerror                                    **
 **
***********************************************************************************/
static int _mme_app_send_sgsap_alert_ack(
  itti_sgsap_alert_request_t *const sgsap_alert_req_pP,
  uint64_t imsi64)
{
  int rc = RETURNerror;
  MessageDef *message_p = NULL;
  itti_sgsap_alert_ack_t *sgsap_alert_ack_pP = NULL;
  OAILOG_FUNC_IN(LOG_MME_APP);

  message_p = itti_alloc_new_message(TASK_MME_APP, SGSAP_ALERT_ACK);
  AssertFatal(message_p, "itti_alloc_new_message Failed");
  sgsap_alert_ack_pP = &message_p->ittiMsg.sgsap_alert_ack;
  memset((void *) sgsap_alert_ack_pP, 0, sizeof(itti_sgsap_alert_ack_t));

  memcpy(
    (void *) sgsap_alert_ack_pP->imsi,
    (const void *) sgsap_alert_req_pP->imsi,
    sgsap_alert_req_pP->imsi_length);
  sgsap_alert_ack_pP->imsi_length = sgsap_alert_req_pP->imsi_length;

  OAILOG_INFO(
    LOG_MME_APP, "Send SGSAP-Alert Reject for IMSI" IMSI_64_FMT " \n", imsi64);
  rc = itti_send_msg_to_task(TASK_SGS, INSTANCE_DEFAULT, message_p);
  OAILOG_FUNC_RETURN(LOG_MME_APP, rc);
}
