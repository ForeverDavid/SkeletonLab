#ifdef use_cgal
#include "skel_mesh_helper.h"
#include <CGAL/intersections.h>

RMesh::Utils::skel_mesh_helper::skel_mesh_helper()
{
    _mesh = NULL;
}

void RMesh::Utils::skel_mesh_helper::setMesh( mesh *m )
{
    _mesh = m;
    RMesh::mesh::getCGALTriangles( (*_mesh), trilist );
    AABB_Tree = new CGAL_AABB_Tree( trilist.begin(), trilist.end( ));
}

void Utils::skel_mesh_helper::checkSkeletonInsideMesh(CurveSkeleton &cs)
{

    for( int i = 0; i < cs.points.size(); ++i)
    {
        if( is_inside( (*AABB_Tree), cs.points[i].coord ))
        {
            cs.points[i].setColor( ColorF::Green());
        }
        else
        {
            cs.points[i].setColor( ColorF::Yellow( ));
        }
    }

}

void Utils::skel_mesh_helper::put_back_inside(CurveSkeleton *cs)
{
    for( int i = 0; i < cs->points.size(); ++i)
    {
        if( !is_inside( (*AABB_Tree), cs->points[i].coord ))
        {
            CGAL_Point test = RMesh::build_CGAL_Point( cs->points[i].coord );

            CGAL_AABB_Tree::Point_and_primitive_id point_and_primitive
                    = AABB_Tree->closest_point_and_primitive( test );
            std::list<CGAL_Triangle>::iterator f = point_and_primitive.second;
            CGAL_Triangle t = *f;
            CGAL_Vector nn = t.supporting_plane().orthogonal_vector() * ( -1.0);
            cs->points[i].coord = build_Point3d( point_and_primitive.first + nn );
        }
    }
}


void Utils::skel_mesh_helper::re_center( CurveSkeleton *&cs, bool refit )
{

    cout << "recentering" << endl;
    map<int, Point3d> new_points;
    map<int, double>  new_radii;
    vector<CGAL_Point>skel_points;
    for (int skel_i = 0; skel_i < cs->points.size(); ++skel_i)
    {
        assert( skel_i == cs->points[skel_i].id );
        skel_points.push_back( build_CGAL_Point(cs->points[skel_i].coord ));
    }

    cout << "points saved # there are " << cs->bones.size() << " bones"  << endl;

    foreach (Skel::Bone b, cs->bones)
    {
        cout << "bone" << endl;
        // start always from a joint
        if ( cs->points[b[0]].isLeaf() ) b.reverse();

        CGAL_Point curr, prev, next;

        for(int i = 0; i < b.size(); ++i )
        {            

            SkelPoint& p = cs->points[b[i]];
            curr = skel_points[b[i]];
            CGAL_Vector skel_dir;

            if( !is_inside( (*AABB_Tree), cs->points[b[i]].coord )) { continue; }

            if( p.isJoint() || p.isLeaf()){
                if( p.isLeaf() ) { skel_dir = skel_points[b.size() - 2] - skel_points[b.size() - 1]; }
                if( p.isJoint())
                {
                    prev = skel_points[b[i-1]];
                    next = skel_points[b[i+1]];
                    CGAL_Vector dir1 = curr - prev,
                                dir2 = next - curr;
                    skel_dir = dir1 + dir2;
                }
                Point3d centroid( 0.0, 0.0, 0.0 );
                double mean_squared_radius;
                CGAL_Plane plane( curr, skel_dir.direction().vector() );
                vector<ir_pair> pairs;
                compute_paired_intersections( plane, curr, pairs);
                int valid_pairs = 0;
                for( int pi = 0; pi < pairs.size(); ++pi ){
                    const ir_pair& pair = pairs[pi];
                    if( !pair.first.is_valid || !pair.second.is_valid ) { continue; }

                    Point3d midpoint = ( build_Point3d( pair.first.point ) + build_Point3d(  pair.second.point )) / 2.0;
                    mean_squared_radius += CGAL::squared_distance( pair.first.point, pair.second.point );
                    centroid += midpoint;
                    ++valid_pairs;
                }
                centroid /= (double)valid_pairs;
                mean_squared_radius /= (double)valid_pairs;
                new_points[b[i]] = centroid;
                new_radii[b[i]] = sqrt(mean_squared_radius)/2.0;
            }
        }
    }
    foreach( Skel::SkelPoint p, cs->points ){
        if( !p.isBranchingNode() ) { continue; }
        if( p.isDeleted())         { continue; }
        if( !is_inside( (*AABB_Tree), p.coord )) { continue; }

        vector<Point3d> centroids;
        vector<double>  radii;
        for( int ni = 0; ni < p.neighbors.size(); ++ni){

            CGAL_Point curr = skel_points[p.id];
            CGAL_Vector skel_dir = skel_points[p.neighbors[ni]] - skel_points[p.id];
            Point3d centroid( 0.0, 0.0, 0.0 );
            double mean_squared_radius;
            CGAL_Plane plane( curr, skel_dir.direction().vector() );
            vector<ir_pair> pairs;
            compute_paired_intersections( plane, curr, pairs);
            int valid_pairs = 0;
            for( int pi = 0; pi < pairs.size(); ++pi ){
                const ir_pair& pair = pairs[pi];
                if( !pair.first.is_valid || !pair.second.is_valid ) { continue; }

                Point3d midpoint = ( build_Point3d( pair.first.point ) + build_Point3d(  pair.second.point )) / 2.0;
                mean_squared_radius += CGAL::squared_distance( pair.first.point, pair.second.point );
                centroid += midpoint;
                ++valid_pairs;
            }
            centroid /= (double)valid_pairs;
            centroids.push_back( centroid );
            radii.push_back( sqrt( mean_squared_radius ) / ( 2.0 * (double)valid_pairs ));
        }
        Point3d centroid( 0.0, 0.0, 0.0 );
        double  mean_squared_radius = 0.0;
        for( int ci = 0; ci < centroids.size(); ++ci){
            centroid += centroids[ci];
            mean_squared_radius += radii[ci];
        }
        centroid /= (double)centroids.size();
        new_points[p.id] = centroid;
        new_radii[p.id ] = ( mean_squared_radius / (double)centroids.size() );
    }


    typedef map<int, Point3d>::iterator mapi;
    for( mapi it = new_points.begin(); it != new_points.end(); ++it )
    {
        assert( new_points.count( it->first ) != 0);
        cs->points[it->first].coord  = it->second;
        if( refit ){
            cs->points[it->first].radius = new_radii[it->first];
            assert( new_radii.count( it->first ) != 0);
        }
    }

}

bool Utils::skel_mesh_helper::is_inside(Utils::CGAL_AABB_Tree &aabb, Point3d query )
{
    CGAL_Point test = RMesh::build_CGAL_Point( query );

    CGAL_AABB_Tree::Point_and_primitive_id point_and_primitive
            = aabb.closest_point_and_primitive( test );

    std::list<CGAL_Triangle>::iterator f = point_and_primitive.second;
    K::Plane_3 p = (*f).supporting_plane();
    return p.has_on_negative_side( test );
}

RMesh::Utils::IntersectionResult Utils::skel_mesh_helper::mesh_ray_intersection( CGAL_Point p, CGAL_Point q )
{
    CGAL_Ray query( p, q );

    std::list<Ray_intersection> intersections;
    AABB_Tree->all_intersections( query,  std::back_inserter( intersections ) );

    float min_dist      = MAXFLOAT;
    CGAL_Point source   = query.source();
    IntersectionResult result;

    for(std::list<Ray_intersection>::iterator it = intersections.begin(); it != intersections.end(); ++it)
    {
        Ray_intersection obj = *it;

        if ( CGAL_Point *curr = boost::get<CGAL_Point>( &( obj->first )))
        {
            CGAL_Point current( curr->x(), curr->y(), curr->z() );
            float dist = CGAL::squared_distance( current, source );
            std::list<CGAL_Triangle>::iterator tit = obj->second;
            CGAL_Triangle t = *tit;

//            if( t.supporting_plane().has_on( source )
//             || t.supporting_plane().has_on_negative_side( source ))
//            {
                if ( dist < min_dist )
                {
                    min_dist        = dist;
                    result.point    = current;
                    result.face     = t;
                    result.is_valid = true;
                }
//            }
        }
    }
    return result;
}


void Utils::skel_mesh_helper::compute_paired_intersections( CGAL_Plane plane, CGAL_Point curr,
    vector<Utils::ir_pair> &pairs){

    std::vector< CGAL_Vector > ray_dirs;
    ray_dirs.push_back( plane.base1().direction().vector( ));
    ray_dirs.push_back( ray_dirs[0] * ( -1.0 ));
    ray_dirs.push_back( plane.base2().direction().vector( ));
    ray_dirs.push_back( ray_dirs[2] * ( -1.0 ));

    ray_dirs.push_back( ray_dirs[0] + ray_dirs[2] );
    ray_dirs.push_back( ray_dirs[4] * ( -1.0 ));
    ray_dirs.push_back( ray_dirs[0] + ray_dirs[3] );
    ray_dirs.push_back( ray_dirs[6] * ( -1.0 ));

    ray_dirs.push_back( ray_dirs[0] + ray_dirs[4] );
    ray_dirs.push_back( ray_dirs[8] * ( -1.0 ));
    ray_dirs.push_back( ray_dirs[4] + ray_dirs[2] );
    ray_dirs.push_back( ray_dirs[10] * ( -1.0 ));

    ray_dirs.push_back( ray_dirs[1] + ray_dirs[7] );
    ray_dirs.push_back( ray_dirs[12] * ( -1.0 ));
    ray_dirs.push_back( ray_dirs[2] + ray_dirs[7] );
    ray_dirs.push_back( ray_dirs[14] * ( -1.0 ));

    double bbox_diag = _mesh->bbox.Diagonal();
    vector<Utils::IntersectionResult> vir;
    for( int ri = 0; ri < ray_dirs.size(); ++ri )
    {
        CGAL_Vector         dir     = ray_dirs[ri];
                            dir     = dir.direction().vector() * bbox_diag;
        CGAL_Point          dest    = curr + dir;
        IntersectionResult  ir      = mesh_ray_intersection( curr, dest );
        if( ir.is_valid )
        {
            if( ir.face.supporting_plane().has_on( curr )
             || ir.face.supporting_plane().has_on_negative_side( curr ))
            {
                vir.push_back(ir);
            }
        }
    }
    for( int ri = 0; ri < vir.size() - 1; ri += 2 )
    {
        pairs.push_back(make_pair( vir[ri], vir[ri+1]));
    }
}

void Utils::skel_mesh_helper::compute_intersections(CGAL_Plane plane,
                                                    CGAL_Point curr, vector<Utils::IntersectionResult> &vir)
{
    std::vector< CGAL_Vector > ray_dirs;
    ray_dirs.push_back( plane.base1().direction().vector( ));
    ray_dirs.push_back( plane.base2().direction().vector( ));
    ray_dirs.push_back( plane.base1().direction().vector() * ( -1.0 ));
    ray_dirs.push_back( plane.base2().direction().vector() * ( -1.0 ));
    ray_dirs.push_back( ray_dirs[0] + ray_dirs[1] );
    ray_dirs.push_back( ray_dirs[1] + ray_dirs[2] );
    ray_dirs.push_back( ray_dirs[2] + ray_dirs[3] );
    ray_dirs.push_back( ray_dirs[3] + ray_dirs[0] );
    ray_dirs.push_back( ray_dirs[0] + ray_dirs[4] );
    ray_dirs.push_back( ray_dirs[1] + ray_dirs[4] );
    ray_dirs.push_back( ray_dirs[1] + ray_dirs[5] );
    ray_dirs.push_back( ray_dirs[5] + ray_dirs[2] );
    ray_dirs.push_back( ray_dirs[2] + ray_dirs[6] );
    ray_dirs.push_back( ray_dirs[6] + ray_dirs[3] );
    ray_dirs.push_back( ray_dirs[3] + ray_dirs[7] );
    ray_dirs.push_back( ray_dirs[7] + ray_dirs[0] );

    double bbox_diag = _mesh->bbox.Diagonal();
    for( int ri = 0; ri < ray_dirs.size(); ++ri )
    {
        CGAL_Vector         dir     = ray_dirs[ri];
                            dir     = dir.direction().vector() * bbox_diag;
        CGAL_Point          dest    = curr + dir;
        IntersectionResult  ir      = mesh_ray_intersection( curr, dest );
        if( ir.is_valid )
        {
            if( ir.face.supporting_plane().has_on( curr )
             || ir.face.supporting_plane().has_on_negative_side( curr ))
            {
                vir.push_back(ir);
            }
        }
    }
}

#endif