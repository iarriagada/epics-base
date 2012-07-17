/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author:          Marty Kraimer
 *                       Andrew Johnson <anj@aps.anl.gov>
 */

#include <stddef.h>
#include <epicsStdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "dbDefs.h"
#include "epicsTime.h"
#include "epicsPrint.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbAddr.h"
#include "db_field_log.h"
#include "errlog.h"
#include "devSup.h"
#include "dbCommon.h"
#include "caeventmask.h"
#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbLink.h"
#include "dbNotify.h"
#include "dbCa.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "recGbl.h"


/* Hook Routines */

RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook = NULL;

/* local routines */
static void getMaxRangeValues(short field_type, double *pupper_limit,
    double *plower_limit);



void recGblDbaddrError(long status, const struct dbAddr *paddr,
    const char *pmessage)
{
    dbCommon *precord = 0;
    dbFldDes	*pdbFldDes = 0;

    if(paddr) {
        pdbFldDes = paddr->pfldDes;
        precord = paddr->precord;
    }
    errPrintf(status,0,0,
        "PV: %s.%s "
        "error detected in routine: %s\n",
        (paddr ? precord->name : "Unknown"),
        (pdbFldDes ? pdbFldDes->name : ""),
        (pmessage ? pmessage : "Unknown"));
    return;
}

void recGblRecordError(long status, void *pdbc,
    const char *pmessage)
{
    dbCommon	*precord = pdbc;

    errPrintf(status,0,0,
        "PV: %s %s\n",
        (precord ? precord->name : "Unknown"),
        (pmessage ? pmessage : ""));
    return;
}

void recGblRecSupError(long status, const struct dbAddr *paddr,
    const char *pmessage, const char *psupport_name)
{
    dbCommon *precord = 0;
    dbFldDes *pdbFldDes = 0;
    dbRecordType *pdbRecordType = 0;

    if(paddr) {
        precord = paddr->precord;
        pdbFldDes = paddr->pfldDes;
        if(pdbFldDes) pdbRecordType = pdbFldDes->pdbRecordType;
    }
    errPrintf(status,0,0,
        "Record Support Routine (%s) "
        "Record Type %s "
        "PV %s.%s "
        " %s\n",
        (psupport_name ? psupport_name : "Unknown"),
        (pdbRecordType ? pdbRecordType->name : "Unknown"),
        (paddr ? precord->name : "Unknown"),
        (pdbFldDes ? pdbFldDes->name : ""),
        (pmessage ? pmessage : ""));
    return;
}

void recGblGetPrec(const struct dbAddr *paddr,long *precision)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;

    switch(pdbFldDes->field_type){
    case(DBF_SHORT):
         *precision = 0;
         break;
    case(DBF_USHORT):
         *precision = 0;
         break;
    case(DBF_LONG):
         *precision = 0;
         break;
    case(DBF_ULONG):
         *precision = 0;
         break;
    case(DBF_FLOAT):
    case(DBF_DOUBLE):
	 if(*precision<0 || *precision>15) *precision=15;
         break;
    default:
         break;
    }
    return;
}

void recGblGetGraphicDouble(
    const struct dbAddr *paddr,struct dbr_grDouble *pgd)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;

    getMaxRangeValues(pdbFldDes->field_type,&pgd->upper_disp_limit,
	&pgd->lower_disp_limit);

    return;
}

void recGblGetAlarmDouble(
    const struct dbAddr *paddr,struct dbr_alDouble *pad)
{
    pad->upper_alarm_limit = 0;
    pad->upper_warning_limit = 0;
    pad->lower_warning_limit = 0;
    pad->lower_alarm_limit = 0;

    return;
}

void recGblGetControlDouble(
    const struct dbAddr *paddr,struct dbr_ctrlDouble *pcd)
{
    dbFldDes *pdbFldDes=paddr->pfldDes;

    getMaxRangeValues(pdbFldDes->field_type,&pcd->upper_ctrl_limit,
	&pcd->lower_ctrl_limit);

    return;
}

int  recGblInitConstantLink(
    struct link *plink,short dbftype,void *pdest)
{
    long status = dbLoadLink(plink, dbftype, pdest);

    if (status)
        return FALSE;
    return TRUE;
}

unsigned short recGblResetAlarms(void *precord)
{
    dbCommon *pdbc = precord;
    epicsEnum16 prev_stat = pdbc->stat;
    epicsEnum16 prev_sevr = pdbc->sevr;
    epicsEnum16 new_stat = pdbc->nsta;
    epicsEnum16 new_sevr = pdbc->nsev;
    epicsEnum16 val_mask = 0;
    epicsEnum16 stat_mask = 0;

    pdbc->stat = new_stat;
    pdbc->sevr = new_sevr;
    pdbc->nsta = 0;
    pdbc->nsev = 0;

    if (prev_sevr != new_sevr) {
	stat_mask = DBE_ALARM;
	db_post_events(pdbc, &pdbc->sevr, DBE_VALUE);
    }
    if (prev_stat != new_stat) {
	stat_mask |= DBE_VALUE;
    }
    if (stat_mask) {
	db_post_events(pdbc, &pdbc->stat, stat_mask);
	val_mask = DBE_ALARM;

	if (!pdbc->ackt || new_sevr >= pdbc->acks) {
	    pdbc->acks = new_sevr;
	    db_post_events(pdbc, &pdbc->acks, DBE_VALUE);
	}

	if (recGblAlarmHook) {
	    (*recGblAlarmHook)(pdbc, prev_sevr, prev_stat);
	}
    }
    return val_mask;
}

int recGblSetSevr(void *precord, epicsEnum16 new_stat, epicsEnum16 new_sevr)
{
    struct dbCommon *prec = precord;
    if (prec->nsev < new_sevr) {
        prec->nsta = new_stat;
	prec->nsev = new_sevr;
	return TRUE;
    }
    return FALSE;
}

void recGblFwdLink(void *precord)
{
    dbCommon *pdbc = precord;

    dbScanFwdLink(&pdbc->flnk);
    /*Handle dbPutFieldNotify record completions*/
    if(pdbc->ppn) dbNotifyCompletion(pdbc);
    if(pdbc->rpro) {
	/*If anyone requested reprocessing do it*/
	pdbc->rpro = FALSE;
	scanOnce(pdbc);
    }
    /*In case putField caused put we are all done */
    pdbc->putf = FALSE;
}

void recGblGetTimeStamp(void *pvoid)
{
    dbCommon* prec = (dbCommon*)pvoid;
    struct link *plink = &prec->tsel;

    if (plink->type != CONSTANT) {
        struct pv_link *ppv_link = &plink->value.pv_link;

        if (ppv_link->pvlMask & pvlOptTSELisTime) {
            if (dbGetTimeStamp(plink, &prec->time))
                errlogPrintf("recGblGetTimeStamp: dbGetTimeStamp failed, %s.TSEL = %s\n",
                    prec->name, ppv_link->pvname);
            return;
        }
        dbGetLink(&prec->tsel, DBR_SHORT, &prec->tse, 0, 0);
    }
    if (prec->tse != epicsTimeEventDeviceTime) {
        if (epicsTimeGetEvent(&prec->time, prec->tse))
            errlogPrintf("recGblGetTimeStamp: epicsTimeGetEvent failed, %s.TSE = %d\n",
                prec->name, prec->tse);
    }
}

void recGblTSELwasModified(struct link *plink)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    char *pfieldname;

    if (plink->type != PV_LINK) {
        errlogPrintf("recGblTSELwasModified called for non PV_LINK\n");
        return;
    }
    /*If pvname ends in .TIME then just ask for VAL*/
    /*Note that the VAL value will not be used*/
    pfieldname = strstr(ppv_link->pvname, ".TIME");
    if (pfieldname) {
        strcpy(pfieldname, ".VAL");
        ppv_link->pvlMask |= pvlOptTSELisTime;
    }
}

static void getMaxRangeValues(short field_type, double *pupper_limit,
    double *plower_limit)
{
    switch(field_type){
    case(DBF_CHAR):
         *pupper_limit = -128.0;
         *plower_limit = 127.0;
         break;
    case(DBF_UCHAR):
         *pupper_limit = 255.0;
         *plower_limit = 0.0;
         break;
    case(DBF_SHORT):
         *pupper_limit = (double)SHRT_MAX;
         *plower_limit = (double)SHRT_MIN;
         break;
    case(DBF_ENUM):
    case(DBF_USHORT):
         *pupper_limit = (double)USHRT_MAX;
         *plower_limit = (double)0;
         break;
    case(DBF_LONG):
	/* long did not work using cast to double */
         *pupper_limit = 2147483647.;
         *plower_limit = -2147483648.;
         break;
    case(DBF_ULONG):
         *pupper_limit = (double)ULONG_MAX;
         *plower_limit = (double)0;
         break;
    case(DBF_FLOAT):
         *pupper_limit = (double)1e+30;
         *plower_limit = (double)-1e30;
         break;
    case(DBF_DOUBLE):
         *pupper_limit = (double)1e+30;
         *plower_limit = (double)-1e30;
         break;
    }
    return;
}