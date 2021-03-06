/* $Header$ */

/* Purpose: NCO utilities for Sparse-1D (S1D) datasets */

/* Copyright (C) 2015--present Charlie Zender
   This file is part of NCO, the netCDF Operators. NCO is free software.
   You may redistribute and/or modify NCO under the terms of the 
   3-Clause BSD License with exceptions described in the LICENSE file */

#include "nco_s1d.h" /* Sparse-1D datasets */

const char * /* O [sng] String describing sparse-type */
nco_s1d_sng /* [fnc] Convert sparse-1D type enum to string */
(const nco_s1d_typ_enm nco_s1d_typ) /* I [enm] Sparse-1D type enum */
{
  /* Purpose: Convert sparse-type enum to string */
  switch(nco_s1d_typ){
  case nco_s1d_clm: return "Sparse Column (cols1d) format";
  case nco_s1d_grd: return "Sparse Gridcell (grid1d) format";
  case nco_s1d_lnd: return "Sparse Landunit (land1d) format";
  case nco_s1d_pft: return "Sparse PFT (pfts1d) format" ;
  default: nco_dfl_case_generic_err(); break;
  } /* !nco_s1d_typ_enm */

  /* Some compilers: e.g., SGI cc, need return statement to end non-void functions */
  return (char *)NULL;
} /* !nco_s1d_sng() */

int /* O [rcd] Return code */
nco_s1d_unpack /* [fnc] Unpack sparse-1D CLM/ELM variables into full file */
(rgr_sct * const rgr, /* I/O [sct] Regridding structure */
 trv_tbl_sct * const trv_tbl) /* I/O [sct] Traversal Table */
{
  /* Purpose: Read sparse CLM/ELM input file, inflate and write into output file */

  /* Usage:
     ncks -O -C --s1d -v cols1d_topoglc ~/data/bm/elm_mali_rst.nc ~/foo.nc
     ncks -O -C --s1d -v cols1d_topoglc --hrz=${DATA}/bm/elm_mali_ig_hst.nc ${DATA}/bm/elm_mali_rst.nc ~/foo.nc */

  const char fnc_nm[]="nco_s1d_unpack()"; /* [sng] Function name */

  char var_nm[NC_MAX_NAME+1L];
  char *fl_in;
  char *fl_out;
  char *fl_tpl; /* [sng] Template file (contains horizontal grid) */

  char dmn_nm[NC_MAX_NAME]; /* [sng] Dimension name */
  char *grd_nm_in=(char *)strdup("gridcell");
  char *lnd_nm_in=(char *)strdup("landunit");
  char *clm_nm_in=(char *)strdup("column");
  char *pft_nm_in=(char *)strdup("pft");

  int dfl_lvl=NCO_DFL_LVL_UNDEFINED; /* [enm] Deflate level */
  int fl_out_fmt=NCO_FORMAT_UNDEFINED; /* [enm] Output file format */
  int fll_md_old; /* [enm] Old fill mode */
  int in_id; /* I [id] Input netCDF file ID */
  int md_open; /* [enm] Mode flag for nc_open() call */
  int out_id; /* I [id] Output netCDF file ID */
  int rcd=NC_NOERR;
  int tpl_id; /* [id] Input netCDF file ID (for horizontal grid template) */

  int dmn_idx; /* [idx] Dimension index */

  /* Initialize local copies of command-line values */
  dfl_lvl=rgr->dfl_lvl;
  fl_in=rgr->fl_in;
  fl_out=rgr->fl_out;
  in_id=rgr->in_id;
  out_id=rgr->out_id;

  /* Search for horizontal grid */
  char *col_nm_in=rgr->col_nm_in; /* [sng] Name to recognize as input horizontal spatial dimension on unstructured grid */
  char *lat_nm_in=rgr->lat_nm_in; /* [sng] Name of input dimension to recognize as latitude */
  char *lon_nm_in=rgr->lon_nm_in; /* [sng] Name of input dimension to recognize as longitude */
  int dmn_id_col_in=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_lat_in=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_lon_in=NC_MIN_INT; /* [id] Dimension ID */

  nco_bool FL_RTR_RMT_LCN;
  nco_bool flg_grd_1D=False; /* [flg] Unpacked data are on unstructured (1D) grid */
  nco_bool flg_grd_rct=False; /* [flg] Unpacked data are on rectangular (2D) grid */
  nco_bool flg_grd_dat=False; /* [flg] Use horizontal grid from required input data file */
  nco_bool flg_grd_tpl=False; /* [flg] Use horizontal grid from optional horizontal grid template file */

  /* Does data file have unstructured grid? 
     MB: Routine must handle two semantically distinct meanings of "column":
     1. The horizontal dimension in an unstructured grid
     2. A fraction of a landunit, which is a fraction of a CTSM/ELM gridcell
        In particular, a column is a fraction of a vegetated, urban, glacier, or crop landunit
     This routine distinguishes these meanings by abbreviating (1) as "col" and (2) as "clm" 
     This usage maintains the precedent that "col" is the horizontal unstructured dimension in nco_rgr.c
     It is necessary though unintuitive that "cols1d" variable metadata will use the "clm" abbreviation */
  if(col_nm_in && (rcd=nco_inq_dimid_flg(in_id,col_nm_in,&dmn_id_col_in)) == NC_NOERR) /* do nothing */; 
  else if((rcd=nco_inq_dimid_flg(in_id,"lndgrid",&dmn_id_col_in)) == NC_NOERR) col_nm_in=strdup("lndgrid"); /* CLM */
  if(dmn_id_col_in != NC_MIN_INT) flg_grd_1D=True;

  /* Does data file have RLL grid? */
  if(!flg_grd_1D){
    if(lat_nm_in && (rcd=nco_inq_dimid_flg(in_id,lat_nm_in,&dmn_id_lat_in)) == NC_NOERR) /* do nothing */; 
    else if((rcd=nco_inq_dimid_flg(in_id,"latitude",&dmn_id_lat_in)) == NC_NOERR) lat_nm_in=strdup("lndgrid"); /* CF */
    if(lon_nm_in && (rcd=nco_inq_dimid_flg(in_id,lon_nm_in,&dmn_id_lon_in)) == NC_NOERR) /* do nothing */; 
    else if((rcd=nco_inq_dimid_flg(in_id,"longitude",&dmn_id_lon_in)) == NC_NOERR) lon_nm_in=strdup("lndgrid"); /* CF */
  } /* !flg_grd_1D */
  if(dmn_id_lat_in != NC_MIN_INT && dmn_id_lon_in != NC_MIN_INT) flg_grd_rct=True;

  /* Set where to obtain horizontal grid */
  if(flg_grd_1D || flg_grd_rct) flg_grd_dat=True; else flg_grd_tpl=True;

  if(flg_grd_tpl && !rgr->fl_hrz){
    (void)fprintf(stderr,"%s: ERROR %s did not locate horizontal grid in input data file and no optional horizontal gridfile was provided.\nHINT: Use option --hrz to specify file with horizontal grid used by input data.\n",nco_prg_nm_get(),fnc_nm);
    nco_exit(EXIT_FAILURE);
  } /* !flg_grd_tpl */

  /* Open grid template file iff necessary */
  if(flg_grd_tpl && rgr->fl_hrz){
    char *fl_pth_lcl=NULL;

    nco_bool HPSS_TRY=False; /* [flg] Search HPSS for unfound files */
    nco_bool RAM_OPEN=False; /* [flg] Open (netCDF3-only) file(s) in RAM */
    nco_bool SHARE_OPEN=rgr->flg_uio; /* [flg] Open (netCDF3-only) file(s) with unbuffered I/O */

    size_t bfr_sz_hnt=NC_SIZEHINT_DEFAULT; /* [B] Buffer size hint */
  
    /* Duplicate (because nco_fl_mk_lcl() free()'s its fl_in) */
    fl_tpl=(char *)strdup(rgr->fl_hrz);
    /* Make sure file is on local system and is readable or die trying */
    fl_tpl=nco_fl_mk_lcl(fl_tpl,fl_pth_lcl,HPSS_TRY,&FL_RTR_RMT_LCN);
    /* Open file using appropriate buffer size hints and verbosity */
    if(RAM_OPEN) md_open=NC_NOWRITE|NC_DISKLESS; else md_open=NC_NOWRITE;
    if(SHARE_OPEN) md_open=md_open|NC_SHARE;
    rcd+=nco_fl_open(fl_tpl,md_open,&bfr_sz_hnt,&tpl_id);

    /* Same logic used to search for grid in data file and to search for grid in template file...
       Does template file have unstructured grid? */
    if(col_nm_in && (rcd=nco_inq_dimid_flg(tpl_id,col_nm_in,&dmn_id_col_in)) == NC_NOERR) /* do nothing */; 
    else if((rcd=nco_inq_dimid_flg(tpl_id,"lndgrid",&dmn_id_col_in)) == NC_NOERR) col_nm_in=strdup("lndgrid"); /* CLM */
    if(dmn_id_col_in != NC_MIN_INT) flg_grd_1D=True;

    /* Does template file have RLL grid? */
    if(!flg_grd_1D){
      if(lat_nm_in && (rcd=nco_inq_dimid_flg(tpl_id,lat_nm_in,&dmn_id_lat_in)) == NC_NOERR) /* do nothing */; 
      else if((rcd=nco_inq_dimid_flg(tpl_id,"latitude",&dmn_id_lat_in)) == NC_NOERR) lat_nm_in=strdup("lndgrid"); /* CF */
      if(lon_nm_in && (rcd=nco_inq_dimid_flg(tpl_id,lon_nm_in,&dmn_id_lon_in)) == NC_NOERR) /* do nothing */; 
      else if((rcd=nco_inq_dimid_flg(tpl_id,"longitude",&dmn_id_lon_in)) == NC_NOERR) lon_nm_in=strdup("lndgrid"); /* CF */
    } /* !flg_grd_1D */
    if(dmn_id_lat_in != NC_MIN_INT && dmn_id_lon_in != NC_MIN_INT) flg_grd_rct=True;

    /* Set where to obtain horizontal grid */
    if(!flg_grd_1D && !flg_grd_rct){
      (void)fprintf(stderr,"%s: ERROR %s did not locate horizontal grid in input data file %s or in template file %s.\nHINT: One of those files must contain the grid dimensions and coordinates used by the packed data in the input data file.\n",nco_prg_nm_get(),fnc_nm,fl_in,fl_tpl);
      nco_exit(EXIT_FAILURE);
    } /* !flg_grd_1D */

  } /* !flg_grd_tpl */

  int cols1d_gridcell_index_id=NC_MIN_INT; /* [id] Gridcell index of column */
  int cols1d_ixy_id=NC_MIN_INT; /* [id] Column 2D longitude index */
  int cols1d_jxy_id=NC_MIN_INT; /* [id] Column 2D latitude index */
  int cols1d_lat_id=NC_MIN_INT; /* [id] Column latitude */
  int cols1d_lon_id=NC_MIN_INT; /* [id] Column longitude */

  int grid1d_ixy_id=NC_MIN_INT; /* [id] Gridcell 2D longitude index */
  int grid1d_jxy_id=NC_MIN_INT; /* [id] Gridcell 2D latitude index */
  int grid1d_lat_id=NC_MIN_INT; /* [id] Gridcell latitude */
  int grid1d_lon_id=NC_MIN_INT; /* [id] Gridcell longitude */

  int land1d_gridcell_index_id=NC_MIN_INT; /* [id] Gridcell index of landunit */
  int land1d_ixy_id=NC_MIN_INT; /* [id] Landunit 2D longitude index */
  int land1d_jxy_id=NC_MIN_INT; /* [id] Landunit 2D latitude index */
  int land1d_lat_id=NC_MIN_INT; /* [id] Landunit latitude */
  int land1d_lon_id=NC_MIN_INT; /* [id] Landunit longitude */

  int pfts1d_gridcell_index_id=NC_MIN_INT; /* [id] Gridcell index of PFT */
  int pfts1d_column_index_id=NC_MIN_INT; /* [id] Column index of PFT */
  int pfts1d_ixy_id=NC_MIN_INT; /* [id] PFT 2D longitude index */
  int pfts1d_jxy_id=NC_MIN_INT; /* [id] PFT 2D latitude index */
  int pfts1d_lat_id=NC_MIN_INT; /* [id] PFT latitude */
  int pfts1d_lon_id=NC_MIN_INT; /* [id] PFT longitude */
  
  int dmn_id_grd=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_lnd=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_clm=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_pft=NC_MIN_INT; /* [id] Dimension ID */

  nco_bool flg_s1d_clm=False; /* [flg] Dataset contains sparse variables for columns */
  nco_bool flg_s1d_grd=False; /* [flg] Dataset contains sparse variables for gridcells */
  nco_bool flg_s1d_lnd=False; /* [flg] Dataset contains sparse variables for landunits */
  nco_bool flg_s1d_pft=False; /* [flg] Dataset contains sparse variables for PFTs */

  rcd=nco_inq_varid_flg(in_id,"cols1d_gridcell_index",&cols1d_gridcell_index_id);
  if(cols1d_gridcell_index_id != NC_MIN_INT) flg_s1d_clm=True;
  if(flg_s1d_clm){
    rcd=nco_inq_varid(in_id,"cols1d_ixy",&cols1d_ixy_id);
    rcd=nco_inq_varid(in_id,"cols1d_jxy",&cols1d_jxy_id);
    rcd=nco_inq_varid(in_id,"cols1d_lat",&cols1d_lat_id);
    rcd=nco_inq_varid(in_id,"cols1d_lon",&cols1d_lon_id);
  } /* !flg_s1d_clm */
     
  rcd=nco_inq_varid_flg(in_id,"grid1d_lat",&grid1d_lat_id);
  if(grid1d_lat_id != NC_MIN_INT) flg_s1d_grd=True;
  if(flg_s1d_grd){
    rcd=nco_inq_varid(in_id,"grid1d_ixy",&grid1d_ixy_id);
    rcd=nco_inq_varid(in_id,"grid1d_jxy",&grid1d_jxy_id);
    rcd=nco_inq_varid(in_id,"grid1d_lon",&grid1d_lon_id);
  } /* !flg_s1d_grd */
     
  rcd=nco_inq_varid_flg(in_id,"land1d_gridcell_index",&land1d_gridcell_index_id);
  if(land1d_gridcell_index_id != NC_MIN_INT) flg_s1d_lnd=True;
  if(flg_s1d_lnd){
    rcd=nco_inq_varid(in_id,"land1d_ixy",&land1d_ixy_id);
    rcd=nco_inq_varid(in_id,"land1d_jxy",&land1d_jxy_id);
    rcd=nco_inq_varid(in_id,"land1d_lat",&land1d_lat_id);
    rcd=nco_inq_varid(in_id,"land1d_lon",&land1d_lon_id);
  } /* !flg_s1d_lnd */
     
  rcd=nco_inq_varid_flg(in_id,"pfts1d_gridcell_index",&pfts1d_gridcell_index_id);
  if(pfts1d_gridcell_index_id != NC_MIN_INT) flg_s1d_pft=True;
  if(flg_s1d_pft){
    rcd=nco_inq_varid(in_id,"pfts1d_column_index",&pfts1d_column_index_id);
    rcd=nco_inq_varid(in_id,"pfts1d_ixy",&pfts1d_ixy_id);
    rcd=nco_inq_varid(in_id,"pfts1d_jxy",&pfts1d_jxy_id);
    rcd=nco_inq_varid(in_id,"pfts1d_lat",&pfts1d_lat_id);
    rcd=nco_inq_varid(in_id,"pfts1d_lon",&pfts1d_lon_id);
  } /* !flg_s1d_pft */
  
  assert(flg_s1d_clm || flg_s1d_lnd || flg_s1d_pft);

  if(flg_s1d_clm) rcd=nco_inq_dimid(in_id,clm_nm_in,&dmn_id_clm);
  if(flg_s1d_grd) rcd=nco_inq_dimid(in_id,grd_nm_in,&dmn_id_grd);
  if(flg_s1d_lnd) rcd=nco_inq_dimid(in_id,lnd_nm_in,&dmn_id_lnd);
  if(flg_s1d_pft) rcd=nco_inq_dimid(in_id,pft_nm_in,&dmn_id_pft);

  if(nco_dbg_lvl_get() >= nco_dbg_std){
    (void)fprintf(stderr,"%s: INFO %s necessary information to unpack cols1d variables\n",nco_prg_nm_get(),flg_s1d_clm ? "Found all" : "Could not find");
    (void)fprintf(stderr,"%s: INFO %s necessary information to unpack lnds1d variables\n",nco_prg_nm_get(),flg_s1d_lnd ? "Found all" : "Could not find");
    (void)fprintf(stderr,"%s: INFO %s necessary information to unpack pfts1d variables\n",nco_prg_nm_get(),flg_s1d_pft ? "Found all" : "Could not find");
  } /* !dbg */

  /* Collect other information from data and template files */
  int dmn_nbr_in; /* [nbr] Number of dimensions in input file */
  int dmn_nbr_out; /* [nbr] Number of dimensions in output file */
  int var_nbr; /* [nbr] Number of variables in file */
  rcd=nco_inq(in_id,&dmn_nbr_in,&var_nbr,(int *)NULL,(int *)NULL);

  const unsigned int trv_nbr=trv_tbl->nbr; /* [idx] Number of traversal table entries */
  int var_cpy_nbr=0; /* [nbr] Number of copied variables */
  int var_rgr_nbr=0; /* [nbr] Number of unpacked variables */
  int var_xcl_nbr=0; /* [nbr] Number of deleted variables */
  int var_crt_nbr=0; /* [nbr] Number of created variables */
  long idx; /* [idx] Generic index */
  unsigned int idx_tbl; /* [idx] Counter for traversal table */

  char *dmn_nm_cp; /* [sng] Dimension name as char * to reduce indirection */
  nco_bool has_clm; /* [flg] Contains column dimension */
  nco_bool has_grd; /* [flg] Contains gridcell dimension */
  nco_bool has_lnd; /* [flg] Contains landunit dimension */
  nco_bool has_pft; /* [flg] Contains PFT dimension */
  nco_bool need_clm=False; /* [flg] At least one variable to unpack needs column dimension */
  nco_bool need_grd=False; /* [flg] At least one variable to unpack needs gridcell dimension */
  nco_bool need_lnd=False; /* [flg] At least one variable to unpack needs landunit dimension */
  nco_bool need_pft=False; /* [flg] At least one variable to unpack needs PFT dimension */ 
  trv_sct trv; /* [sct] Traversal table object structure to reduce indirection */
  /* Define unpacking flag for each variable */
  for(idx_tbl=0;idx_tbl<trv_nbr;idx_tbl++){
    trv=trv_tbl->lst[idx_tbl];
    if(trv.nco_typ == nco_obj_typ_var && trv.flg_xtr){
      dmn_nbr_in=trv_tbl->lst[idx_tbl].nbr_dmn;
      has_clm=False;
      has_grd=False;
      has_lnd=False;
      has_pft=False;
      for(dmn_idx=0;dmn_idx<dmn_nbr_in;dmn_idx++){
	/* Pre-determine flags necessary during next loop */
	dmn_nm_cp=trv.var_dmn[dmn_idx].dmn_nm;
	if(!has_clm && clm_nm_in) has_clm=!strcmp(dmn_nm_cp,clm_nm_in);
	if(!has_grd && grd_nm_in) has_grd=!strcmp(dmn_nm_cp,grd_nm_in);
	if(!has_lnd && lnd_nm_in) has_lnd=!strcmp(dmn_nm_cp,lnd_nm_in);
	if(!has_pft && pft_nm_in) has_pft=!strcmp(dmn_nm_cp,pft_nm_in);
      } /* !dmn_idx */
      /* Unpack variables that contain a sparse-1D dimension */
      if(has_clm || has_lnd || has_pft){
	trv_tbl->lst[idx_tbl].flg_rgr=True;
	var_rgr_nbr++;
	if(has_clm) need_clm=True;
	if(has_grd) need_grd=True;
	if(has_lnd) need_lnd=True;
	if(has_pft) need_pft=True;
      } /* endif */
      assert(!(has_clm && has_lnd));
      assert(!(has_clm && has_pft));
      assert(!(has_lnd && has_pft));
      /* Copy all variables that are not regridded or omitted */
      if(!trv_tbl->lst[idx_tbl].flg_rgr) var_cpy_nbr++;
    } /* end nco_obj_typ_var */
  } /* end idx_tbl */
  if(!var_rgr_nbr) (void)fprintf(stdout,"%s: WARNING %s reports no variables fit unpacking criteria. The sparse data unpacker expects at least one variable to unpack, and variables not unpacked are copied straight to output. HINT: If the name(s) of the input sparse-1D dimensions (e.g., \"column\", \"landunit\", and \"pft\") do not match NCO's preset defaults (case-insensitive unambiguous forms and abbreviations of \"column\", \"landunit\", and/or \"pft\", respectively) then change the dimension names that NCO looks for. Instructions are at http://nco.sf.net/nco.html#sparse. For CTSM/ELM sparse-1D coordinate grids, ensure that the \"column\" and \"landunit\" variable names are known with, e.g., \"ncks --rgr column_nm=clm --rgr landunit_nm=lnd\" or \"ncremap -R '--rgr clm=clm --rgr lnd=lnd'\".\n",nco_prg_nm_get(),fnc_nm);
  if(nco_dbg_lvl_get() >= nco_dbg_fl){
    for(idx_tbl=0;idx_tbl<trv_nbr;idx_tbl++){
      trv=trv_tbl->lst[idx_tbl];
      if(trv.nco_typ == nco_obj_typ_var && trv.flg_xtr) (void)fprintf(stderr,"Unpack %s? %s\n",trv.nm,trv.flg_rgr ? "Yes" : "No");
    } /* end idx_tbl */
  } /* end dbg */

  int hrz_id; /* [id] Horizontal grid netCDF file ID */
  long col_nbr; /* [nbr] Number of columns */
  long lon_nbr; /* [nbr] Number of longitudes */
  long lat_nbr; /* [nbr] Number of latitudes */
  if(flg_grd_dat) hrz_id=in_id; else hrz_id=tpl_id;
  if(flg_grd_1D) rcd=nco_inq_dimlen(hrz_id,dmn_id_col_in,&col_nbr);
  if(flg_grd_rct){
    rcd=nco_inq_dimlen(hrz_id,dmn_id_lat_in,&lat_nbr);
    rcd=nco_inq_dimlen(hrz_id,dmn_id_lon_in,&lon_nbr);
  } /* !flg_grd_rct */

  if(flg_grd_tpl){
    nco_bool RM_RMT_FL_PST_PRC=True; /* Option R */

    /* No further access to template file, close it */
    nco_close(tpl_id);

    /* Remove local copy of file */
    if(FL_RTR_RMT_LCN && RM_RMT_FL_PST_PRC) (void)nco_fl_rm(fl_tpl);
  } /* !flg_grd_tpl */

  /* Lay-out unpacked file */
  char *col_nm_out=NULL;
  char *lat_nm_out=NULL;
  char *lon_nm_out=NULL;
  char *lat_dmn_nm_out;
  char *lon_dmn_nm_out;
  int dmn_id_col_out=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_lat_out=NC_MIN_INT; /* [id] Dimension ID */
  int dmn_id_lon_out=NC_MIN_INT; /* [id] Dimension ID */
  int col_out_id; /* [id] Variable ID for column */
  int lon_out_id; /* [id] Variable ID for longitude */
  int lat_out_id; /* [id] Variable ID for latitude */

  if(rgr->col_nm_out) col_nm_out=rgr->col_nm_out; else col_nm_out=col_nm_in;
  if(rgr->lat_dmn_nm) lat_dmn_nm_out=rgr->lat_dmn_nm; else lat_dmn_nm_out=lat_nm_in;
  if(rgr->lon_dmn_nm) lon_dmn_nm_out=rgr->lon_dmn_nm; else lon_dmn_nm_out=lon_nm_in;
  if(rgr->lat_nm_out) lat_nm_out=rgr->lat_nm_out; else lat_nm_out=lat_nm_in;
  if(rgr->lon_nm_out) lon_nm_out=rgr->lon_nm_out; else lon_nm_out=lon_nm_in;

  /* Define horizontal dimensions before all else */
  if(flg_grd_1D){
    rcd=nco_def_dim(out_id,col_nm_out,col_nbr,&dmn_id_col_out);
  } /* !flg_grd_1D */
  if(flg_grd_rct){
    rcd=nco_def_dim(out_id,lat_nm_out,lat_nbr,&dmn_id_lat_out);
    rcd=nco_def_dim(out_id,lon_nm_out,lon_nbr,&dmn_id_lon_out);
  } /* !flg_grd_rct */

  /* Pre-allocate dimension ID and cnt/srt space */
  int *dmn_id_in=NULL; /* [id] Dimension IDs */
  int *dmn_id_out=NULL; /* [id] Dimension IDs */
  int dmn_nbr_max; /* [nbr] Maximum number of dimensions variable can have in input or output */

  long *dmn_cnt_in=NULL;
  long *dmn_cnt_out=NULL;
  long *dmn_srt=NULL;
  rcd+=nco_inq_ndims(in_id,&dmn_nbr_max);
  dmn_id_in=(int *)nco_malloc(dmn_nbr_max*sizeof(int));
  dmn_id_out=(int *)nco_malloc(dmn_nbr_max*sizeof(int));
  if(dmn_srt) dmn_srt=(long *)nco_free(dmn_srt);
  dmn_srt=(long *)nco_malloc(dmn_nbr_max*sizeof(long));
  if(dmn_cnt_in) dmn_cnt_in=(long *)nco_free(dmn_cnt_in);
  if(dmn_cnt_out) dmn_cnt_out=(long *)nco_free(dmn_cnt_out);
  dmn_cnt_in=(long *)nco_malloc(dmn_nbr_max*sizeof(long));
  dmn_cnt_out=(long *)nco_malloc(dmn_nbr_max*sizeof(long));

  //(void)fprintf(stdout,"%s: DEBUG quark1 dmn_nbr_out = %d, dmn_nbr_ps = %d\n",nco_prg_nm_get(),dmn_nbr_out,dmn_nbr_ps);

  int shuffle; /* [flg] Turn-on shuffle filter */
  int deflate; /* [flg] Turn-on deflate filter */
  deflate=(int)True;
  shuffle=NC_SHUFFLE;
  dfl_lvl=rgr->dfl_lvl;
  fl_out_fmt=rgr->fl_out_fmt;

  const int dmn_nbr_0D=0; /* [nbr] Rank of 0-D grid variables (scalars) */
  const int dmn_nbr_1D=1; /* [nbr] Rank of 1-D grid variables */
  const nc_type crd_typ_out=NC_DOUBLE;
  nc_type var_typ_rgr; /* [enm] Variable type used during regridding */
  var_typ_rgr=NC_DOUBLE; /* NB: Perform interpolation in double precision */

  if(flg_grd_1D){
    rcd+=nco_def_var(out_id,col_nm_out,crd_typ_out,dmn_nbr_1D,&dmn_id_col_out,&col_out_id);
    if(dfl_lvl > 0) (void)nco_def_var_deflate(out_id,col_out_id,shuffle,deflate,dfl_lvl);
    var_crt_nbr++;
  } /* !flg_grd_1D */
  if(flg_grd_rct){
    rcd+=nco_def_var(out_id,lat_nm_out,crd_typ_out,dmn_nbr_1D,&dmn_id_lat_out,&lat_out_id);
    if(dfl_lvl > 0) (void)nco_def_var_deflate(out_id,lat_out_id,shuffle,deflate,dfl_lvl);
    var_crt_nbr++;
    rcd+=nco_def_var(out_id,lon_nm_out,crd_typ_out,dmn_nbr_1D,&dmn_id_lon_out,&lon_out_id);
    if(dfl_lvl > 0) (void)nco_def_var_deflate(out_id,lon_out_id,shuffle,deflate,dfl_lvl);
    var_crt_nbr++;
  } /* !flg_grd_rct */
  
  /* Free pre-allocated array space */
  if(dmn_id_in) dmn_id_in=(int *)nco_free(dmn_id_in);
  if(dmn_id_out) dmn_id_out=(int *)nco_free(dmn_id_out);
  if(dmn_srt) dmn_srt=(long *)nco_free(dmn_srt);
  if(dmn_cnt_in) dmn_cnt_in=(long *)nco_free(dmn_cnt_in);
  if(dmn_cnt_out) dmn_cnt_out=(long *)nco_free(dmn_cnt_out);

  /* Unpack and copy data from input file */
  int dmn_idx_col=int_CEWI; /* [idx] Index of column dimension */
  int dmn_idx_lat=int_CEWI; /* [idx] Index of latitude dimension */
  int dmn_idx_lon=int_CEWI; /* [idx] Index of longitude dimension */
  int var_id; /* [id] Current variable ID */

  long var_sz; /* [nbr] Size of variable */

  nc_type var_typ_in; /* [enm] NetCDF type of input data */
  nc_type var_typ_out; /* [enm] NetCDF type of data in output file */

  nco_s1d_typ_enm nco_s1d_typ; /* [enm] Sparse-1D type of input variable */

  ptr_unn var_val_in;
  ptr_unn var_val_out;
  
#ifdef ENABLE_S1D

#ifdef __GNUG__
# pragma omp parallel for firstprivate(has_clm,has_grd,has_lnd,has_pft,var_val_in,var_val_out) private(dmn_cnt_in,dmn_cnt_out,dmn_id_in,dmn_id_out,dmn_idx,dmn_nbr_in,dmn_nbr_out,dmn_nbr_max,dmn_nm,dmn_srt,grd_idx,has_mss_val,idx_in,idx_out,idx_tbl,in_id,lvl_idx_in,lvl_idx_out,lvl_nbr_in,lvl_nbr_out,mss_val_dbl,rcd,thr_idx,trv,var_id_in,var_id_out,var_nm,var_sz_in,var_sz_out,var_typ_out,var_typ_rgr) shared(dmn_id_col_in,dmn_id_col_out,dmn_id_pft_in,dmn_id_pft_out,dmn_id_tm_in,flg_s1d_clm,flg_s1d_pft,grd_nbr,idx_dbg,col_nbr_in,col_nbr_out,pft_nbr_in,pft_nbr_out,out_id,xtr_mth)
#endif /* !__GNUG__ */
  for(idx_tbl=0;idx_tbl<trv_nbr;idx_tbl++){
    trv=trv_tbl->lst[idx_tbl];
    thr_idx=omp_get_thread_num();
    in_id=trv_tbl->in_id_arr[thr_idx];
#ifdef _OPENMP
    if(nco_dbg_lvl_get() >= nco_dbg_grp && !thr_idx && !idx_tbl) (void)fprintf(fp_stdout,"%s: INFO %s reports regrid loop uses %d thread%s\n",nco_prg_nm_get(),fnc_nm,omp_get_num_threads(),(omp_get_num_threads() > 1) ? "s" : "");
    if(nco_dbg_lvl_get() >= nco_dbg_var) (void)fprintf(fp_stdout,"%s: INFO thread = %d, idx_tbl = %d, nm = %s\n",nco_prg_nm_get(),thr_idx,idx_tbl,trv.nm);
#endif /* !_OPENMP */
    if(trv.nco_typ == nco_obj_typ_var && trv.flg_xtr){
      if(nco_dbg_lvl_get() >= nco_dbg_var) (void)fprintf(fp_stdout,"%s%s ",trv.flg_rgr ? "#" : "~",trv.nm);
      if(trv.flg_rgr){
	/* Interpolate variable */
	
	if(strstr("cols1d",var_nm)){
	  nco_s1d_typ=nco_s1d_clm;
	}else if(strstr("pfts1d",var_nm)){
	  nco_s1d_typ=nco_s1d_pft;
	}else{
	  (void)fprintf(stderr,"%s: ERROR %s reports variable %s does not appear to be sparse\n",nco_prg_nm_get(),fnc_nm,var_nm);
	  nco_exit(EXIT_FAILURE);
	} /* !strstr() */

	if(nco_dbg_lvl_get() >= nco_dbg_std){
	  (void)fprintf(stderr,"%s: INFO %s reports variable %s is sparse type %s",nco_prg_nm_get(),fnc_nm,var_nm,nco_s1d_sng(nco_s1d_typ));
	} /* !dbg */
	
      }else{ /* !trv.flg_rgr */
	/* Use standard NCO copy routine for variables that are not regridded
	   20190511: Copy them only once */
#pragma omp critical
	{ /* begin OpenMP critical */
	  (void)nco_cpy_var_val(in_id,out_id,(FILE *)NULL,(md5_sct *)NULL,trv.nm,trv_tbl);
	} /* end OpenMP critical */
      } /* !flg_rgr */
    } /* !xtr */
  } /* end (OpenMP parallel for) loop over idx_tbl */
  if(nco_dbg_lvl_get() >= nco_dbg_var) (void)fprintf(stdout,"\n");
  if(nco_dbg_lvl_get() >= nco_dbg_fl) (void)fprintf(stdout,"%s: INFO %s completion report: Variables interpolated = %d, copied unmodified = %d, omitted = %d, created = %d\n",nco_prg_nm_get(),fnc_nm,var_rgr_nbr,var_cpy_nbr,var_xcl_nbr,var_crt_nbr);

  /* Free output data memory */
#endif /* !ENABLE_S1D */

  //  if(col_nm_in) col_nm_in=(char *)nco_free(col_nm_in);
  if(clm_nm_in) clm_nm_in=(char *)nco_free(clm_nm_in);
  if(grd_nm_in) grd_nm_in=(char *)nco_free(grd_nm_in);
  if(lnd_nm_in) lnd_nm_in=(char *)nco_free(lnd_nm_in);
  if(pft_nm_in) pft_nm_in=(char *)nco_free(pft_nm_in);

  return rcd;
} /* !nco_s1d_unpack() */
