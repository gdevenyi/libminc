#include  <minc.h>
#include  <def_mni.h>

#define  INVALID_AXIS   -1

private  void  create_world_transform(
    Point       *origin,
    Vector      axes[N_DIMENSIONS],
    Real        axis_spacing[N_DIMENSIONS],
    Transform   *transform );
private  int  match_dimension_names(
    int               n_volume_dims,
    String            *volume_dimension_names,
    int               n_file_dims,
    String            *file_dimension_names,
    int               axis_index_in_file[] );

public  Minc_file  initialize_minc_input(
    char       filename[],
    Volume     volume )
{
    minc_file_struct    *file;
    int                 img_var, dim_vars[MAX_VAR_DIMS];
    long                long_size;
    Boolean             converted_sign;
    nc_type             converted_type;
    String              signed_flag;
    String              dim_names[MAX_VAR_DIMS];
    nc_type             file_datatype;
    int                 sizes[MAX_VAR_DIMS];
    double              separation[MAX_VAR_DIMS];
    Real                axis_separation[MI_NUM_SPACE_DIMS];
    double              start_position[MAX_VAR_DIMS];
    double              dir_cosines[MAX_VAR_DIMS][MI_NUM_SPACE_DIMS];
    Vector              axes[MI_NUM_SPACE_DIMS];
    Boolean             spatial_dim_flags[MAX_VAR_DIMS];
    Vector              offset;
    Point               origin;
    int                 d, dimvar, which_valid_axis;
    int                 spatial_axis_indices[MAX_VAR_DIMS];

    ALLOC( file, 1 );

    file->file_is_being_read = TRUE;
    file->volume = volume;

    ncopts = 0;
    file->cdfid =  ncopen( filename, NC_NOWRITE );
    ncopts = NC_VERBOSE | NC_FATAL;

    if( file->cdfid == MI_ERROR )
    {
        print( "Error: opening MINC file \"%s\".\n", filename );
        return( (Minc_file) 0 );
    }

    img_var = ncvarid( file->cdfid, MIimage );

    ncvarinq( file->cdfid, img_var, (char *) NULL, &file_datatype,
              &file->n_file_dimensions, dim_vars, (int *) NULL );

    if( file->n_file_dimensions < volume->n_dimensions )
    {
        print( "Error: MINC file has only %d dims, volume requires %d.\n",
               file->n_file_dimensions, volume->n_dimensions );
        return( (Minc_file) 0 );
    }
    else if( file->n_file_dimensions > MAX_VAR_DIMS )
    {
        print( "Error: MINC file has %d dims, can only handle %d.\n",
               file->n_file_dimensions, MAX_VAR_DIMS );
        return( (Minc_file) 0 );
    }

    for_less( d, 0, file->n_file_dimensions )
    {
        (void) ncdiminq( file->cdfid, dim_vars[d], dim_names[d],
                         &long_size );
        file->sizes_in_file[d] = long_size;
    }

    if( !match_dimension_names( volume->n_dimensions, volume->dimension_names,
                                file->n_file_dimensions, dim_names,
                                file->axis_index_in_file ) )
    {
        print( "Error:  dimension names did not match: \n" );
        
        print( "\n" );
        print( "Requested:\n" );
        for_less( d, 0, volume->n_dimensions )
            print( "%d: %s\n", d+1, volume->dimension_names[d] );

        print( "\n" );
        print( "In File:\n" );
        for_less( d, 0, file->n_file_dimensions )
            print( "%d: %s\n", d+1, dim_names[d] );
    }

    file->n_volumes_in_file = 1;
    which_valid_axis = 0;

    for_less( d, 0, file->n_file_dimensions )
    {
        if( strcmp( dim_names[d], MIxspace ) == 0 )
            spatial_axis_indices[d] = X;
        else if( strcmp( dim_names[d], MIyspace ) == 0 )
            spatial_axis_indices[d] = Y;
        else if( strcmp( dim_names[d], MIzspace ) == 0 )
            spatial_axis_indices[d] = Z;
        else
            spatial_axis_indices[d] = INVALID_AXIS;

        spatial_dim_flags[d] = (spatial_axis_indices[d] != INVALID_AXIS);

        if( file->axis_index_in_file[d] != INVALID_AXIS )
        {
            file->valid_file_axes[which_valid_axis] = d;
            ++which_valid_axis;
        }
    }

    for_less( d, 0, file->n_file_dimensions )
    {
        separation[d] = 1.0;
        start_position[d] = 0.0;

        if( spatial_dim_flags[d] )
        {
            dir_cosines[d][0] = 0.0;
            dir_cosines[d][1] = 0.0;
            dir_cosines[d][2] = 0.0;
            dir_cosines[d][spatial_axis_indices[d]] = 1.0;
        }

        ncopts = 0;
        dimvar = ncvarid( file->cdfid, dim_names[d] );
        if( dimvar != MI_ERROR )
        {
            (void) miattget1( file->cdfid, dimvar, MIstep, NC_DOUBLE,
                              (void *) (&separation[d]) );

            if( spatial_dim_flags[d] )
            {
                 (void) miattget1( file->cdfid, dimvar, MIstart, NC_DOUBLE,
                                   (void *) (&start_position[d]) );
                 (void) miattget( file->cdfid, dimvar, MIdirection_cosines,
                                  NC_DOUBLE, MI_NUM_SPACE_DIMS,
                                  (void *) (dir_cosines[d]), (int *) NULL );
            }
        }
        ncopts = NC_VERBOSE | NC_FATAL;

        if( file->axis_index_in_file[d] == INVALID_AXIS )
        {
            file->n_volumes_in_file *= file->sizes_in_file[d];
        }
        else
        {
            sizes[file->axis_index_in_file[d]] = file->sizes_in_file[d];
            volume->separation[file->axis_index_in_file[d]] = separation[d];
        }
    }

    fill_Vector( axes[X], 1.0, 0.0, 0.0 );
    fill_Vector( axes[Y], 0.0, 1.0, 0.0 );
    fill_Vector( axes[Z], 0.0, 0.0, 1.0 );

    axis_separation[X] = 1.0;
    axis_separation[Y] = 1.0;
    axis_separation[Z] = 1.0;

    fill_Point( origin, 0.0, 0.0, 0.0 );

    for_less( d, 0, file->n_file_dimensions )
    {
        if( spatial_axis_indices[d] != INVALID_AXIS )
        {
            fill_Vector( axes[spatial_axis_indices[d]],
                         dir_cosines[d][0],
                         dir_cosines[d][1],
                         dir_cosines[d][2] );
            NORMALIZE_VECTOR( axes[spatial_axis_indices[d]],
                              axes[spatial_axis_indices[d]] );
            
            SCALE_VECTOR( offset, axes[spatial_axis_indices[d]],
                          start_position[d] );
            ADD_POINT_VECTOR( origin, origin, offset );

            axis_separation[spatial_axis_indices[d]] = separation[d];
        }
    }

    create_world_transform( &origin, axes, axis_separation,
                            &volume->voxel_to_world_transform );
    compute_transform_inverse( &volume->voxel_to_world_transform,
                               &volume->world_to_voxel_transform );

    if( volume->data_type == NO_DATA_TYPE )
    {
        ncopts = 0;
        (void) ncattget( file->cdfid, img_var, MIsigntype,
                         (void *) signed_flag );
        ncopts = NC_VERBOSE | NC_FATAL;
        converted_sign = (strcmp( signed_flag, MI_SIGNED ) == 0);

        converted_type = file_datatype;
    }
    else
    {
        converted_type = volume->nc_data_type;
        converted_sign = volume->signed_flag;
    }

    set_volume_size( volume, converted_type, converted_sign, sizes );

    file->icv = miicv_create();

    (void) miicv_setint( file->icv, MI_ICV_TYPE, converted_type );
    (void) miicv_setstr( file->icv, MI_ICV_SIGN,
                         converted_sign ? MI_SIGNED : MI_UNSIGNED );

    (void) miicv_attach( file->icv, file->cdfid, img_var );

/*
    volume->value_scale = (real_max - real_min) / 255.0;
    volume->value_translation = real_min;
*/
    volume->value_scale = 1.0;
    volume->value_translation = 0.0;

    for_less( d, 0, file->n_file_dimensions )
        file->input_indices[d] = 0;

    file->end_volume_flag = FALSE;

    return( file );
}

public  int  get_n_input_volumes(
    Minc_file  file )
{
    return( file->n_volumes_in_file );
}

public  int  get_minc_input_dimensions()
{
}

public  int  close_minc_input(
    Minc_file   file )
{
    if( file == (Minc_file) NULL )
    {
        print( "close_minc_input(): NULL file.\n" );
        return( MI_ERROR );
    }

    (void) ncclose( file->cdfid );
    (void) miicv_free( file->icv );

    FREE( file );

    return( MI_NOERROR );
}

private  void  input_slab(
    Minc_file   file,
    Volume      volume,
    long        start[],
    long        count[] )
{
    int      i, valid_ind, file_ind;
    int      iv[MAX_DIMENSIONS], n_to_read;
    void     *void_ptr;
    char     *buffer;

    n_to_read = 1;

    for_less( i, 0, file->n_file_dimensions )
         n_to_read *= count[i];

    ALLOC( buffer, sizeof(double) * n_to_read );
    void_ptr = (void *) buffer;

    (void) miicv_get( file->icv, start, count, void_ptr );

    for_less( i, 0, volume->n_dimensions )
    {
        file_ind = file->valid_file_axes[i];
        iv[file->axis_index_in_file[file_ind]] = start[file_ind];
    }

    for_less( i, 0, n_to_read )
    {
        switch( volume->data_type )
        {
        case UNSIGNED_BYTE:
            SET_VOXEL( volume, iv[0], iv[1], iv[2], iv[3], iv[4],
                       ( (unsigned char *) void_ptr )[i] );
            break;
        }

        valid_ind = volume->n_dimensions-1;

        while( i >= 0 )
        {
            file_ind = file->valid_file_axes[valid_ind];
            if( count[file_ind] > 1 )
            {
                ++iv[file->axis_index_in_file[file_ind]];
                if( iv[file->axis_index_in_file[file_ind]] <
                    start[file_ind] + count[file_ind])
                    break;
                iv[file->axis_index_in_file[file_ind]] = start[file_ind];
            }

            --valid_ind;
        }
    }

    FREE( buffer );
}

public  int  input_more_minc_file(
    Minc_file   file )
{
    int      ind, file_ind, fastest_varying;
    long     start[MAX_VAR_DIMS], count[MAX_VAR_DIMS];
    Volume   volume;

    if( file->end_volume_flag )
        return( FALSE );

    volume = file->volume;

    if( volume->data == (void *) NULL )
        alloc_volume_data( volume );

    for_less( ind, 0, file->n_file_dimensions )
    {
        count[ind] = 1;
        start[ind] = file->input_indices[ind];
    }

    fastest_varying = file->valid_file_axes[volume->n_dimensions-1];
    count[fastest_varying] = file->sizes_in_file[fastest_varying];

    input_slab( file, volume, start, count );

    /* advance to next 1D strip */

    ind = volume->n_dimensions-2;

    while( ind >= 0 )
    {
        file_ind = file->valid_file_axes[ind];

        ++file->input_indices[file_ind];
        if( file->input_indices[file_ind] < file->sizes_in_file[file_ind] )
            break;

        file->input_indices[file_ind] = 0;
        --ind;
    }

    if( ind < 0 )
        file->end_volume_flag = TRUE;

    return( !file->end_volume_flag );
}

public  int  advance_volume(
    Minc_file   file )
{
    int   ind;

    ind = file->n_file_dimensions-1;

    while( ind >= 0 )
    {
        if( file->axis_index_in_file[ind] == INVALID_AXIS )
        {
            ++file->input_indices[ind];
            if( file->input_indices[ind] < file->sizes_in_file[ind] )
                break;

            file->input_indices[ind] = 0;
        }
        --ind;
    }

    if( ind >= 0 )
    {
        file->end_volume_flag = FALSE;

        for_less( ind, 0, file->volume->n_dimensions )
        {
            file->input_indices[file->valid_file_axes[ind]] = 0;
        }

    }
    else
        file->end_volume_flag = TRUE;

    return( file->end_volume_flag );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : create_world_transform
@INPUT      : origin        - point origin
              axes          - 3 vectors
              axis_spacing  - voxel spacing
@OUTPUT     : transform
@RETURNS    : 
@DESCRIPTION: Using the information from the mnc file, creates a 4 by 4
              transform which converts a voxel to world space.
              Voxel centres are at integer numbers in voxel space.  So the
              bottom left voxel is (0.0,0.0,0.0).
@CREATED    :                      David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

private  void  create_world_transform(
    Point       *origin,
    Vector      axes[N_DIMENSIONS],
    Real        axis_spacing[N_DIMENSIONS],
    Transform   *transform )
{
    Vector   x_axis, y_axis, z_axis;

    x_axis = axes[X];
    y_axis = axes[Y];
    z_axis = axes[Z];

    SCALE_VECTOR( x_axis, x_axis, axis_spacing[X] );
    SCALE_VECTOR( y_axis, y_axis, axis_spacing[Y] );
    SCALE_VECTOR( z_axis, z_axis, axis_spacing[Z] );

    make_change_to_bases_transform( origin, &x_axis, &y_axis, &z_axis,
                                    transform );
}

private  Boolean  is_spatial_dimension(
    char   dimension_name[] )
{
    return( strcmp(dimension_name,MIxspace) == 0 ||
            strcmp(dimension_name,MIyspace) == 0 ||
            strcmp(dimension_name,MIzspace) == 0 );
}

private  int  match_dimension_names(
    int               n_volume_dims,
    String            *volume_dimension_names,
    int               n_file_dims,
    String            *file_dimension_names,
    int               axis_index_in_file[] )
{
    int      i, j, iteration, n_matches;
    Boolean  match;
    Boolean  volume_dim_found[MAX_DIMENSIONS];

    n_matches = 0;

    for_less( i, 0, n_file_dims )
        axis_index_in_file[i] = INVALID_AXIS;

    for_less( i, 0, n_volume_dims )
        volume_dim_found[i] = FALSE;

    for_less( iteration, 0, 3 )
    {
        for( i = n_volume_dims-1;  i >= 0;  --i )
        {
            if( !volume_dim_found[i] )
            {
                for( j = n_file_dims-1;  j >= 0;  --j )
                {
                    if( axis_index_in_file[j] == INVALID_AXIS )
                    {
                        switch( iteration )
                        {
                        case 0:
                            match = strcmp( volume_dimension_names[i],
                                             file_dimension_names[j] ) == 0;
                            break;
                        case 1:
                            match = (strcmp( volume_dimension_names[i],
                                            ANY_SPATIAL_DIMENSION ) == 0) &&
                                is_spatial_dimension( file_dimension_names[j] );
                            break;
                        case 2:
                            match = (strlen(volume_dimension_names[i]) == 0);
                            break;
                        }

                        if( match )
                        {
                            axis_index_in_file[j] = i;
                            volume_dim_found[i] = TRUE;
                            ++n_matches;
                            break;
                        }
                    }
                }
            }
        }
    }

    return( n_matches == n_volume_dims );
}
