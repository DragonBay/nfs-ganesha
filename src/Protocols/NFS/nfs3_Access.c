/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---------------------------------------
 */

/**
 * \file    nfs3_Access.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:49 $
 * \version $Revision: 1.11 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs3_Access.c : Routines used for managing the NFS4 COMPOUND functions.
 *
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#include "log.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_proto_tools.h"
#include "nfs_tools.h"

/**
 * nfs2_Access: Implements NFSPROC3_ACCESS.
 *
 * Implements NFSPROC3_ACCESS.
 *
 * @param[in]  parg     NFS arguments union
 * @param[in]  pexport  NFS export list
 * @param[in]  pcontext Credentials to be used for this request
 * @param[in]  pworker  Worker thread data
 * @param[in]  preq     SVC request related to this call
 * @param[out] pres     Structure to contain the result of the call
 *
 * @retval NFS_REQ_OK if successful
 * @retval NFS_REQ_DROP if failed but retryable
 * @retval NFS_REQ_FAILED if failed and not retryable
 *
 */

int nfs3_Access(nfs_arg_t *parg,
                exportlist_t *pexport,
                fsal_op_context_t *pcontext,
                nfs_worker_data_t *pworker,
                struct svc_req *preq,
                nfs_res_t *pres)
{
  cache_inode_status_t cache_status;
  cache_entry_t *pentry = NULL;
  cache_inode_fsal_data_t fsal_data;
  fsal_attrib_list_t attr;
  int rc = NFS_REQ_OK;

  if(isDebug(COMPONENT_NFSPROTO))
    {
      char                  str[LEN_FH_STR];
      struct display_buffer dspbuf = {sizeof(str), str, str};

      (void) display_fhandle3(&dspbuf, (nfs_fh3 *) parg);

      LogDebug(COMPONENT_NFSPROTO,
               "REQUEST PROCESSING: Calling nfs3_Access handle: %s", str);
    }

  /* Is this a xattr FH ? */
  if(nfs3_Is_Fh_Xattr(&(parg->arg_access3.object)))
    {
      rc = nfs3_Access_Xattr(parg, pexport, pcontext, preq, pres);
      goto out;
    }

  /* to avoid setting it on each error case */
  pres->res_access3.ACCESS3res_u.resfail.obj_attributes.attributes_follow
    = FALSE;

  /* Convert file handle into a fsal_handle */
  if(nfs3_FhandleToFSAL(&(parg->arg_access3.object),
                        &fsal_data.fh_desc, pcontext) == 0)
    {
      rc = NFS_REQ_DROP;
      goto out;
    }

  /* Get the entry in the cache_inode */
  if((pentry = cache_inode_get(&fsal_data,
                               &attr,
                               pcontext,
                               NULL,
                               &cache_status)) == NULL)
    {
        goto out_error;
    }

  /* Perform the 'access' call */
  nfs_access_op(pentry,
                parg->arg_access3.access,
                &pres->res_access3.ACCESS3res_u.resok.access,
                NULL,
                pcontext,
                &attr,
                &cache_status);

  if(cache_status == CACHE_INODE_SUCCESS ||
     cache_status == CACHE_INODE_FSAL_EACCESS)
    {
      /* Build Post Op Attributes */
      nfs_SetPostOpAttr(pexport,
                        &attr,
                        &(pres->res_access3.ACCESS3res_u.resok.obj_attributes));

      pres->res_access3.status = NFS3_OK;
      rc = NFS_REQ_OK;
      goto out;
    }

out_error:

  /* If we are here, there was an error */
  rc = nfs_SetFailedStatus(pexport,
                           NFS_V3,
                           cache_status,
                           NULL,
                           &pres->res_access3.status,
                           &(pres->res_access3.ACCESS3res_u.resfail.obj_attributes),
                           NULL, NULL, NULL, NULL);

out:

  if (pentry)
    {
      cache_inode_put(pentry);
    }

  return rc;
}                               /* nfs3_Access */

/**
 * nfs3_Access_Free: Frees the result structure allocated for nfs3_Access.
 * 
 * Frees the result structure allocated for nfs3_Access.
 * 
 * @param pres        [INOUT]   Pointer to the result structure.
 *
 */
void nfs3_Access_Free(nfs_res_t * pres)
{
  /* Nothing to do */
  return;
}                               /* nfs3_Access_Free */