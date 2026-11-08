// ============================================================================
// Brush.inc - tile brushes for the Mapper (first brush: Road).
// Include once from Mapper.cpp, directly AFTER   #include "MapGen.inc"
// (it reuses the Mg* drawing helpers and the MapGenPreviewActive undo guard).
//
// How it works
//  * "Brush" button (under MapGen) opens a small panel: pick Road, pick size 1..7.
//  * With the panel open, LEFT-DRAG on the map paints road (default cursor).
//  * Nothing is stored except the ORIGINAL tiles of the cells you touched (for Cancel).
//  * Every touched cell is re-resolved from its 4 neighbours, so the new road joins
//    existing road tiles automatically (merge).
//  * Apply = ONE undo step (Ctrl+Z).  Cancel = restore the touched cells.  Closing the
//    panel applies.  While a stroke is pending, Ctrl+Z is blocked (same as MapGen).
//
// Brush footprint = size + 1 cells  (size 1 = 2x2 = 4 corners; size 3 = 4x4 ...).
// Change BR_FOOTPRINT_ADD if you want a different relation.
// ============================================================================

#define BR_MAX_SIZE       ( 7 )
#define BR_FOOTPRINT_ADD  ( 1 )
#define BR_PANEL_W        ( 172 )
#define BR_SPRAY_MAX      ( 10 )
#define BR_SPREAD_MAX     ( 20 )

// ---- road tile set --------------------------------------------------------------
// index -> file.  Order is fixed, see BrResolve.
//  0 solid | 1 NW corner | 2,3 N | 4 NE corner | 5,6 E | 7 SE corner | 8,9 S | 10 SW corner | 11,12 W
// 13..16 = inner (bend) corners, dirt only at one vertex:  13 = left vertex (53), 14 = top (51), 15 = right (52), 16 = bottom (50)
// index order of every road set: 0 solid | 1 NW corner | 2,3 N | 4 NE corner | 5,6 E | 7 SE corner | 8,9 S | 10 SW corner | 11,12 W | 13..16 inner NW,NE,SE,SW
static const int         BR_ROAD_NUM = 17;
static const int         BR_SET_NUM = 2;
static const int         BR_ROAD_NUMBERS[ 2 ][ 17 ] =
{
    { 2, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 53, 51, 52, 50 },      // Road   (clean asphalt)
    { 2, 69, 17, 18, 70, 20, 21, 72, 11, 12, 71, 14, 15, 16, 19, 10, 13 },      // Road 2 (rubble edge): sides 17/18 N, 20/21 E, 11/12 S, 14/15 W; inner 16 left, 19 top, 10 right, 13 bottom
};

// Compass -> tile-grid neighbour.  Order N, E, S, W.
// Art on screen: N = top-left edge, E = top-right, S = bottom-right, W = bottom-left
// (NW corner = left vertex, NE = top, SE = right, SW = bottom).
// Grid: ty+1 = down-right, tx+1 = down-left.  So N = ty-1, S = ty+1, E = tx-1, W = tx+1.
// If it is still rotated, change ONLY these two arrays.
static const int         BR_DX[ 4 ] = { 0, -1, 0, 1 };
static const int         BR_DY[ 4 ] = { -1, 0, 1, 0 };

// 1 = a bend (road on all 4 sides, one diagonal missing) gets the inner-corner tile road50..53
#define BR_INNER_CORNERS  ( 1 )

struct BrState
{
    bool                                     Visible;
    bool                                     Active;        // a stroke session is in the map (not yet applied)
    int                                      Tool;          // 0 = off, 1 = Road, 2 = Eraser, 3 = Spray
    int                                      Size;
    bool                                     Dragging;
    int                                      LastTx, LastTy;
    ProtoMap*                                Map;
    uint                                     Hash[ 17 ];
    bool                                     HashOk;
    uint                                     SetHash[ 2 ][ 17 ];
    bool                                     SetOk[ 2 ];
    int                                      RoadSet;       // set used for painting (0 Road, 1 Road 2)
    std::set< uint >                         RoadAny;       // every road tile of every set: what counts as 'road' on the map
    bool                                     Dirty;
    bool                                     Erased;        // the eraser deleted something: Cancel restores the baseline         // RebuildTiles needed
    std::map< uint, ProtoMap::TileVec >      Orig;          // touched cell (ty<<16|tx) -> tiles before the first change
    std::string                              Status;
    // Spray
    int                                      SprayCount;    // assets placed per dab (1..10)
    int                                      Spread;        // radius of a dab, in hexes (1..20)
    int                                      SKind;         // 0 = none, 1 = item / scenery, 2 = tile
    ushort                                   SPid;
    uint                                     SHash;
    bool                                     SRoof;
    int                                      SLayer;
    bool                                     Picking;       // next map click is the pipette
    int                                      LastDabX, LastDabY;
    std::vector< MapObject* >                SprObjs;       // objects sprayed in this session
    std::map< uint, ProtoMap::TileVec >      SprTileOrig;   // (roof<<31|hy<<16|hx) -> tiles before the first spray
    std::set< uint >                         SprUsed;       // hexes already sprayed in this session

    BrState(): Visible( false ), Active( false ), Tool( 1 ), Size( 3 ), Dragging( false ), LastTx( -1 ), LastTy( -1 ),
               Map( NULL ), HashOk( false ), RoadSet( 0 ), Dirty( false ), SprayCount( 3 ), Spread( 4 ), SKind( 0 ), SPid( 0 ), SHash( 0 ),
               SRoof( false ), SLayer( 0 ), Picking( false ), LastDabX( -1 ), LastDabY( -1 ) { memset( Hash, 0, sizeof( Hash ) ); memset( SetHash, 0, sizeof( SetHash ) ); SetOk[ 0 ] = SetOk[ 1 ] = false; Erased = false; }
};
static BrState BR;

// panel layout: rows depend on the tool (Spray adds Pick and Spread rows)
static int BrPanelH() { return 24 + 120 + ( BR.Tool == 3 ? 60 : 0 ) + 30 + 20 + 30 + 6; }

struct BrLay { int road, road2, eraser, spray, pick, spread, size, status, btn; };
static BrLay BrLayout( int py )
{
    BrLay L;
    int   y = py + 24;
    L.road = y;   y += 30;
    L.road2 = y;  y += 30;
    L.eraser = y; y += 30;
    L.spray = y;  y += 30;
    if( BR.Tool == 3 )
    {
        L.pick = y;   y += 30;
        L.spread = y; y += 30;
    }
    else
        L.pick = L.spread = -1;
    L.size = y;   y += 30;
    L.status = y; y += 20;
    L.btn = y;
    return L;
}

static inline uint BrKey( int tx, int ty ) { return ( (uint) ty << 16 ) | (uint) tx; }

// ---- name -> hash (taken from the Mapper's own tile list, so paths always match) ----
static std::string BrLower( const std::string& s )
{
    std::string r = s;
    for( size_t i = 0; i < r.size(); i++ )
        if( r[ i ] >= 'A' && r[ i ] <= 'Z' )
            r[ i ] = (char) ( r[ i ] - 'A' + 'a' );
    return r;
}

static void BrUseSet( int set )
{
    if( set < 0 || set >= BR_SET_NUM || !BR.SetOk[ set ] )
        return;
    BR.RoadSet = set;
    memcpy( BR.Hash, BR.SetHash[ set ], sizeof( BR.Hash ) );
}

static bool BrInitHashes( FOMapper* m )
{
    if( BR.HashOk )
        return true;
    uint byNum[ 100 ];
    memset( byNum, 0, sizeof( byNum ) );
    for( auto it = m->Tabs[ INT_MODE_TILE ].begin(); it != m->Tabs[ INT_MODE_TILE ].end(); ++it )
    {
        FOMapper::SubTab& st = it->second;
        for( size_t i = 0; i < st.TileNames.size() && i < st.TileHashes.size(); i++ )
        {
            const std::string n = BrLower( st.TileNames[ i ] );
            if( n.size() < 10 )
                continue;
            const std::string t = n.substr( n.size() - 10 );          // "roadNN.frm"
            if( t.compare( 0, 4, "road" ) ||
    ( t.compare( 6, 4, ".frm" ) && t.compare( 6, 4, ".png" ) ) ||
    t[ 4 ] < '0' || t[ 4 ] > '9' ||
    t[ 5 ] < '0' || t[ 5 ] > '9' )
    continue;
            const int num = ( t[ 4 ] - '0' ) * 10 + ( t[ 5 ] - '0' );
            if( !byNum[ num ] )
                byNum[ num ] = st.TileHashes[ i ];
        }
    }
    BR.RoadAny.clear();
    for( int set = 0; set < BR_SET_NUM; set++ )
    {
        int found = 0;
        for( int k = 0; k < BR_ROAD_NUM; k++ )
        {
            BR.SetHash[ set ][ k ] = byNum[ BR_ROAD_NUMBERS[ set ][ k ] ];
            if( BR.SetHash[ set ][ k ] )
                found++;
        }
        BR.SetOk[ set ] = ( found > 0 );                                // missing tiles do not block the brush: those spots stay empty
        for( int k = 0; k < BR_ROAD_NUM; k++ )
            if( BR.SetHash[ set ][ k ] )
                BR.RoadAny.insert( BR.SetHash[ set ][ k ] );
        if( found < BR_ROAD_NUM )
        {
            std::string miss;
            char        one[ 16 ];
            for( int k = 0; k < BR_ROAD_NUM; k++ )
                if( !BR.SetHash[ set ][ k ] )
                {
                    sprintf( one, "road%02d", BR_ROAD_NUMBERS[ set ][ k ] );
                    if( miss.find( one ) == std::string::npos )
                        miss += ( miss.empty() ? "" : ", " ) + std::string( one );
                }
            m->AddMessFormat( "Brush: %s - %d tile(s) missing, those spots stay empty: %s", set ? "Road 2" : "Road", BR_ROAD_NUM - found, miss.c_str() );
        }
    }
    BR.HashOk = BR.SetOk[ 0 ];                                          // Road 2 is optional
    if( !BR.HashOk )
        m->AddMess( "Brush: no Road tiles found in the tile list." );
    if( BR.HashOk )
        BrUseSet( 0 );
    return BR.HashOk;
}

// ---- map access --------------------------------------------------------------------
static bool BrInside( ProtoMap* pm, int tx, int ty )
{
    return tx >= 0 && ty >= 0 && tx < (int) pm->Header.MaxHexX / 2 && ty < (int) pm->Header.MaxHexY / 2;
}

static bool BrIsRoadMap( ProtoMap* pm, int tx, int ty )
{
    if( !BrInside( pm, tx, ty ) )
        return false;
    const ProtoMap::TileVec& tv = pm->GetTiles( (ushort) ( tx * 2 ), (ushort) ( ty * 2 ), false );
    for( size_t i = 0; i < tv.size(); i++ )
        if( BR.RoadAny.count( tv[ i ].NameHash ) )
            return true;
    return false;
}

// which set a road cell belongs to (tiles that exist in both sets - solid and corners - count for the active one)
static int BrCellSet( ProtoMap* pm, int tx, int ty )
{
    const ProtoMap::TileVec& tv = pm->GetTiles( (ushort) ( tx * 2 ), (ushort) ( ty * 2 ), false );
    for( size_t i = 0; i < tv.size(); i++ )
    {
        bool in0 = false, in1 = false;
        for( int k = 0; k < BR_ROAD_NUM; k++ )
        {
            if( tv[ i ].NameHash == BR.SetHash[ 0 ][ k ] ) in0 = true;
            if( tv[ i ].NameHash == BR.SetHash[ 1 ][ k ] ) in1 = true;
        }
        if( in0 && !in1 ) return 0;
        if( in1 && !in0 ) return 1;
    }
    return BR.RoadSet;
}

// remove everything from a floor cell (map data + hex field)
static void BrClearCell( FOMapper* m, ProtoMap* pm, int tx, int ty )
{
    const ushort hx = (ushort) ( tx * 2 ), hy = (ushort) ( ty * 2 );
    pm->GetTiles( hx, hy, false ).clear();
    m->HexMngr.GetField( hx, hy ).Tiles.clear();
}

static void BrPutHash( FOMapper* m, ProtoMap* pm, int tx, int ty, uint hash )
{
    if( !hash )
        return;                                         // tile missing from the set: the cell stays empty
    m->HexMngr.SetTile( hash, (ushort) ( tx * 2 ), (ushort) ( ty * 2 ), 0, 0, DRAW_ORDER_TILE, false, false );
}

// ---- resolver: which of the 13 tiles fits this cell --------------------------------
static int BrVariant( int tx, int ty )
{
    uint h = (uint) tx * 73856093u ^ (uint) ty * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (int) ( ( h >> 3 ) & 1 );
}

static bool BrRoadAt( ProtoMap* pm, int x, int y, const std::set< uint >& add )
{
    return BrInside( pm, x, y ) && ( add.count( BrKey( x, y ) ) || BrIsRoadMap( pm, x, y ) );
}

// corner tile index for the pair of compass sides (d1,d2)
static int BrCornerOf( int d1, int d2 )
{
    const int a = d1 < d2 ? d1 : d2, b = d1 < d2 ? d2 : d1;
    if( a == 0 && b == 3 ) return 1;    // N+W  -> NW
    if( a == 0 && b == 1 ) return 4;    // N+E  -> NE
    if( a == 1 && b == 2 ) return 7;    // E+S  -> SE
    return 10;                          // S+W  -> SW
}

// `add` = cells the current stroke is turning into road (not yet written)
static int BrResolve( ProtoMap* pm, int tx, int ty, const std::set< uint >& add )
{
    bool open[ 4 ];
    int  n = 0;
    for( int d = 0; d < 4; d++ )
    {
        open[ d ] = !BrRoadAt( pm, tx + BR_DX[ d ], ty + BR_DY[ d ], add );
        n += open[ d ] ? 1 : 0;
    }
    const int v = BrVariant( tx, ty );
    if( n == 1 )
    {
        const int d = open[ 0 ] ? 0 : ( open[ 1 ] ? 1 : ( open[ 2 ] ? 2 : 3 ) );
        return 2 + d * 3 + v;
    }
    if( n == 2 )
    {
        int p[ 2 ], k = 0;
        for( int d = 0; d < 4; d++ )
            if( open[ d ] )
                p[ k++ ] = d;
        if( p[ 1 ] - p[ 0 ] != 2 )                      // adjacent sides = a real corner
            return BrCornerOf( p[ 0 ], p[ 1 ] );
        return 0;                                       // 1-wide strip: no tile for it
    }
    if( n == 3 )                                        // tip of a spur: corner at one of its two ends
    {
        int closed = 0;                                 // the one side that still has road
        for( int d = 0; d < 4; d++ )
            if( !open[ d ] )
                closed = d;
        const int d1 = ( closed + 1 + v ) & 3, d2 = ( closed + 2 + v ) & 3;   // two neighbouring open sides
        return BrCornerOf( d1, d2 );
    }
#if BR_INNER_CORNERS
    if( n == 0 )                                        // bend: road on all 4 sides, a diagonal is missing
    {
        int missing = 0, md = -1;
        for( int d = 0; d < 4; d++ )
        {
            const int e = ( d + 1 ) & 3;
            if( !BrRoadAt( pm, tx + BR_DX[ d ] + BR_DX[ e ], ty + BR_DY[ d ] + BR_DY[ e ], add ) )
            {
                missing++;
                md = d;
            }
        }
        if( missing == 1 )
            return 13 + BrCornerOf( md, ( md + 1 ) & 3 ) / 3;   // corner idx 1,4,7,10 -> inner idx 13..16
    }
#endif
    return 0;
}

// ---- paint -------------------------------------------------------------------------
// Close one-cell dents and one-cell-wide gaps next to the stroke: an empty cell with road on 3 sides,
// or on two opposite sides, becomes road.  Repeats a few times so stepped notches fill up as well.
#define BR_FILL_GAPS  ( 1 )
static void BrCloseGaps( ProtoMap* pm, std::set< uint >& add )
{
    for( int pass = 0; pass < 4; pass++ )
    {
        std::set< uint > cand, grow;
        for( std::set< uint >::const_iterator it = add.begin(); it != add.end(); ++it )
        {
            const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
            for( int oy = -2; oy <= 2; oy++ )
                for( int ox = -2; ox <= 2; ox++ )
                    if( BrInside( pm, tx + ox, ty + oy ) && !BrRoadAt( pm, tx + ox, ty + oy, add ) )
                        cand.insert( BrKey( tx + ox, ty + oy ) );
        }
        for( std::set< uint >::const_iterator it = cand.begin(); it != cand.end(); ++it )
        {
            const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
            bool r[ 4 ];
            int  k = 0;
            for( int d = 0; d < 4; d++ )
            {
                // only the stroke's own cells count: old road must not pull the gap shut (that made 1-cell "bridges")
                r[ d ] = BrInside( pm, tx + BR_DX[ d ], ty + BR_DY[ d ] ) && add.count( BrKey( tx + BR_DX[ d ], ty + BR_DY[ d ] ) );
                k += r[ d ] ? 1 : 0;
            }
            if( k >= 3 || ( r[ 0 ] && r[ 2 ] ) || ( r[ 1 ] && r[ 3 ] ) )
                grow.insert( *it );
        }
        // pinch points: road on all 4 sides but 2+ diagonals empty (no tile exists) -> fill one of those diagonals
        for( std::set< uint >::const_iterator it = add.begin(); it != add.end(); ++it )
        {
            const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
            bool all4 = true;
            for( int d = 0; d < 4 && all4; d++ )
                all4 = BrRoadAt( pm, tx + BR_DX[ d ], ty + BR_DY[ d ], add );
            if( !all4 )
                continue;
            int miss[ 4 ], mc = 0;
            for( int d = 0; d < 4; d++ )
            {
                const int e = ( d + 1 ) & 3;
                const int dx = tx + BR_DX[ d ] + BR_DX[ e ], dy = ty + BR_DY[ d ] + BR_DY[ e ];
                if( BrInside( pm, dx, dy ) && !BrRoadAt( pm, dx, dy, add ) )
                    miss[ mc++ ] = d;
            }
            if( mc >= 2 )
            {
                const int d = miss[ 0 ], e = ( d + 1 ) & 3;
                grow.insert( BrKey( tx + BR_DX[ d ] + BR_DX[ e ], ty + BR_DY[ d ] + BR_DY[ e ] ) );
            }
        }
        if( grow.empty() )
            break;
        add.insert( grow.begin(), grow.end() );
    }
}

static void BrApplyCells( FOMapper* m, ProtoMap* pm, const std::set< uint >& add_in )
{
    std::set< uint > add( add_in );
#if BR_FILL_GAPS
    BrCloseGaps( pm, add );
#endif
    std::set< uint > touch( add );
    for( std::set< uint >::const_iterator it = add.begin(); it != add.end(); ++it )
    {
        const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
        for( int oy = -1; oy <= 1; oy++ )
            for( int ox = -1; ox <= 1; ox++ )
            {
                const int nx = tx + ox, ny = ty + oy;
                if( ( ox || oy ) && BrInside( pm, nx, ny ) && !add.count( BrKey( nx, ny ) ) && BrIsRoadMap( pm, nx, ny ) )
                    touch.insert( BrKey( nx, ny ) );    // existing road next to the stroke: re-resolve (merge)
            }
    }
    // backup first (only first touch of a cell), then resolve against the unchanged neighbours
    std::vector< std::pair< uint, int > > plan;
    for( std::set< uint >::const_iterator it = touch.begin(); it != touch.end(); ++it )
    {
        const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
        if( !BR.Orig.count( *it ) )
            BR.Orig[ *it ] = pm->GetTiles( (ushort) ( tx * 2 ), (ushort) ( ty * 2 ), false );
        plan.push_back( std::make_pair( *it, BrResolve( pm, tx, ty, add ) ) );
    }
    for( size_t i = 0; i < plan.size(); i++ )
    {
        const int tx = (int) ( plan[ i ].first & 0xFFFF ), ty = (int) ( plan[ i ].first >> 16 );
        BrClearCell( m, pm, tx, ty );
        BrPutHash( m, pm, tx, ty, BR.Hash[ plan[ i ].second ] );
    }
    BR.Dirty = true;
}

// ---- eraser: deletes whatever the main panel's Select filters allow (Items, Scenery, Walls, Critters, Floor, Roof)
// inside the brush fields. It uses the Mapper's own selection + delete, so it acts exactly like a drag-select followed
// by Delete. Nothing is re-resolved or transformed.
static void BrEraseCells( FOMapper* m, ProtoMap* pm, const std::set< uint >& cells )
{
    (void) pm;
    ItemHexVec items;
    CritVec    critters;
    for( std::set< uint >::const_iterator it = cells.begin(); it != cells.end(); ++it )
    {
        const int tx = (int) ( *it & 0xFFFF ), ty = (int) ( *it >> 16 );
        for( int oy = 0; oy < 2; oy++ )                 // one field = 2x2 hexes
            for( int ox = 0; ox < 2; ox++ )
            {
                const ushort hx = (ushort) ( tx * 2 + ox ), hy = (ushort) ( ty * 2 + oy );
                m->HexMngr.GetItems( hx, hy, items );
                m->HexMngr.GetCritters( hx, hy, critters, FIND_ALL );
                if( m->IsSelectTile && GameOpt.ShowTile )
                    m->SelectAddTile( hx, hy, false );
                if( m->IsSelectRoof && GameOpt.ShowRoof )
                    m->SelectAddTile( hx, hy, true );
            }
    }
    for( uint k = 0; k < items.size(); k++ )
    {
        const ushort pid = items[ k ]->GetProtoId();
        if( m->HexMngr.IsIgnorePid( pid ) )
            continue;
        if( !GameOpt.ShowFast && m->HexMngr.IsFastPid( pid ) )
            continue;
        if( items[ k ]->IsItem() && m->IsSelectItem && GameOpt.ShowItem )
            m->SelectAddItem( items[ k ] );
        else if( items[ k ]->IsScenOrGrid() && m->IsSelectScen && GameOpt.ShowScen )
            m->SelectAddItem( items[ k ] );
        else if( items[ k ]->IsWall() && m->IsSelectWall && GameOpt.ShowWall )
            m->SelectAddItem( items[ k ] );
        else if( GameOpt.ShowFast && m->HexMngr.IsFastPid( pid ) )
            m->SelectAddItem( items[ k ] );
    }
    for( uint l = 0; l < critters.size(); l++ )
        if( m->IsSelectCrit && GameOpt.ShowCrit )
            m->SelectAddCrit( critters[ l ] );

    if( m->SelectedObj.empty() && m->SelectedTile.empty() )
        return;
    m->SelectDelete();
    BR.Erased = true;
    BR.Dirty = true;
}

static void BrStamp( ProtoMap* pm, int cx, int cy, std::set< uint >& add )
{
    const int f = BR.Size + ( BR.Tool == 2 ? 0 : BR_FOOTPRINT_ADD );   // Eraser: size 1 = 1 field
    const int x0 = cx - ( f - 1 ) / 2, y0 = cy - ( f - 1 ) / 2;
    for( int y = y0; y < y0 + f; y++ )
        for( int x = x0; x < x0 + f; x++ )
            if( BrInside( pm, x, y ) )
                add.insert( BrKey( x, y ) );
}

// cursor -> tile cell
static bool BrCursorCell( FOMapper* m, int& tx, int& ty )
{
    ushort hx, hy;
    if( !m->HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
        return false;
    tx = hx >> 1;
    ty = hy >> 1;
    return true;
}

// stroke from the last cell to this one (straight line on the tile grid)
static void BrStroke( FOMapper* m, int tx, int ty )
{
    ProtoMap* pm = m->CurProtoMap;
    std::set< uint > add;
    int x = BR.LastTx < 0 ? tx : BR.LastTx, y = BR.LastTy < 0 ? ty : BR.LastTy;
    const int dx = abs( tx - x ), dy = abs( ty - y );
    const int sx = x < tx ? 1 : -1, sy = y < ty ? 1 : -1;
    int err = dx - dy;
    for( ;; )
    {
        BrStamp( pm, x, y, add );
        if( x == tx && y == ty )
            break;
        const int e2 = 2 * err;
        if( e2 > -dy ) { err -= dy; x += sx; }
        if( e2 < dx )  { err += dx; y += sy; }
    }
    BR.LastTx = tx;
    BR.LastTy = ty;
    if( !add.empty() )
    {
        if( BR.Tool == 2 )
            BrEraseCells( m, pm, add );
        else
            BrApplyCells( m, pm, add );
    }
}

// ---- session -----------------------------------------------------------------------
static bool BrMapOk( FOMapper* m ) { return m->CurProtoMap && m->HexMngr.IsMapLoaded(); }

// ---- spray: scatter the picked asset around the cursor -----------------------------------
static bool BrSprayCapture( FOMapper* m )
{
    char buf[ 96 ];
    if( m->IsObjectMode() && m->CurItemProtos && m->CurItemProtos->size() )
    {
        BR.SKind = 1;
        BR.SPid = ( *m->CurItemProtos )[ m->GetTabIndex() ].ProtoId;
        sprintf( buf, "Asset: object %d", (int) BR.SPid );
    }
    else if( m->IsTileMode() && m->CurTileHashes && m->CurTileHashes->size() )
    {
        BR.SKind = 2;
        BR.SHash = ( *m->CurTileHashes )[ m->GetTabIndex() ];
        BR.SRoof = m->DrawRoof;
        BR.SLayer = m->TileLayer;
        sprintf( buf, "Asset: tile %u", BR.SHash );
    }
    else
    {
        BR.SKind = 0;
        BR.Status = "Only objects and tiles";
        return false;
    }
    BR.Status = buf;
    return true;
}

static void BrSprayRemoveObjs( FOMapper* m )
{
    ProtoMap* pm = BR.Map ? BR.Map : m->CurProtoMap;
    for( size_t i = 0; i < BR.SprObjs.size(); i++ )
    {
        MapObject* o = BR.SprObjs[ i ];
        if( pm )
        {
            std::vector< MapObject* >::iterator it = std::find( pm->MObjects.begin(), pm->MObjects.end(), o );
            if( it != pm->MObjects.end() )
                pm->MObjects.erase( it );
        }
        m->HexMngr.FinishItem( o->RunTime.MapObjId, 0 );
        SAFEREL( o );
    }
    if( !BR.SprObjs.empty() )
        m->HexMngr.RefreshMap();
    BR.SprObjs.clear();
}

static double BrRnd() { return (double) rand() / ( (double) RAND_MAX + 1.0 ); }

static void BrSprayDab( FOMapper* m )
{
    ProtoMap* pm = m->CurProtoMap;
    ushort    chx, chy;
    if( !m->HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, chx, chy ) )
        return;
    if( !BR.SKind )
    {
        BR.Status = "Pick an asset first";
        return;
    }
    BR.LastDabX = chx;
    BR.LastDabY = chy;

    // hex -> screen is a 2x2 linear map (calibrated by the minimap code); invert it to scatter in a screen circle
    const double det = (double) MiniXA * MiniYB - (double) MiniXB * MiniYA;
    if( det > -0.001 && det < 0.001 )
        return;
    const int maxX = m->HexMngr.GetMaxHexX(), maxY = m->HexMngr.GetMaxHexY();
    for( int i = 0; i < BR.SprayCount; i++ )
    {
        const double r = BR.Spread * 16.0 * sqrt( BrRnd() ), a = BrRnd() * 6.283185307;
        const double X = r * cos( a ), Y = r * sin( a );
        const int    hx = chx + (int) floor( ( MiniYB * X - MiniXB * Y ) / det + 0.5 );
        const int    hy = chy + (int) floor( ( -MiniYA * X + MiniXA * Y ) / det + 0.5 );
        if( hx < 0 || hy < 0 || hx >= maxX || hy >= maxY )
            continue;

        if( BR.SKind == 1 )
        {
            if( !BR.SprUsed.insert( BrKey( hx, hy ) ).second )
                continue;                               // one asset per hex, later dabs fill the gaps
            const size_t before = pm->MObjects.size();
            if( m->ParseProto( BR.SPid, (ushort) hx, (ushort) hy, NULL ) )
                for( size_t k = before; k < pm->MObjects.size(); k++ )
                    BR.SprObjs.push_back( pm->MObjects[ k ] );
        }
        else
        {
            const int ex = hx & ~1, ey = hy & ~1;
            if( !BR.SprUsed.insert( BrKey( ex, ey ) ).second )
                continue;
            const uint key = ( BR.SRoof ? 0x80000000u : 0u ) | ( (uint) ey << 16 ) | (uint) ex;
            if( !BR.SprTileOrig.count( key ) )
                BR.SprTileOrig[ key ] = pm->GetTiles( (ushort) ex, (ushort) ey, BR.SRoof );
            m->ParseTile( BR.SHash, (ushort) ex, (ushort) ey, 0, 0, (uchar) BR.SLayer, BR.SRoof );
            BR.Dirty = true;
        }
    }
}

static void BrStart( FOMapper* m )
{
    ProtoMap*    pmap = m->CurProtoMap;
    UndoHistory* hist = UndoFind( pmap );
    if( !hist )
        UndoCreate( pmap );
    else if( UndoDirty )
        UndoCommit( m, pmap, hist );                    // baseline for undo, taken BEFORE the first change
    m->SelectClear();
    BR.Map = pmap;
    BR.Erased = false;
    BR.Orig.clear();
    BR.SprObjs.clear();
    BR.SprTileOrig.clear();
    BR.SprUsed.clear();
    BR.Active = true;
    MapGenPreviewActive = true;
    BR.Status = "Drawing - Apply keeps it";
}

static void BrEnd()
{
    BR.Active = false;
    BR.Dragging = false;
    BR.Map = NULL;
    BR.Erased = false;
    BR.Orig.clear();
    BR.SprObjs.clear();
    BR.SprTileOrig.clear();
    BR.SprUsed.clear();
    BR.Picking = false;
    BR.LastDabX = BR.LastDabY = -1;
    BR.LastTx = BR.LastTy = -1;
    BR.Dirty = false;
    MapGenPreviewActive = false;
}

static void BrApply( FOMapper* m )
{
    if( !BR.Active )
        return;
    const size_t n = BR.Orig.size() + BR.SprObjs.size() + BR.SprTileOrig.size();
    if( BR.Dirty )
        m->HexMngr.RebuildTiles();
    BrEnd();
    UndoMarkDirty();                                    // exactly one undo step for the whole stroke set
    m->AddMessFormat( "Brush: applied (%d cells). Ctrl+Z undoes it.", (int) n );
}

static void BrCancel( FOMapper* m )
{
    if( !BR.Active )
        return;
    if( BR.Erased && BrMapOk( m ) && BR.Map == m->CurProtoMap )
    {
        // deleted objects cannot be re-created one by one: put the whole map back to the state taken before drawing
        ProtoMap*    pm = m->CurProtoMap;
        UndoHistory* h = UndoFind( pm );
        if( h && UndoApply( m, pm, h->States[ h->Cur ] ) )
        {
            UndoRefreshLive( pm, h );
            BR.SprObjs.clear();                         // those pointers died with the rebuild
            BR.SprTileOrig.clear();
            BrEnd();
            m->AddMess( "Brush: cancelled, map restored to the state before drawing." );
            return;
        }
    }
    if( BrMapOk( m ) && BR.Map == m->CurProtoMap )
    {
        ProtoMap* pm = m->CurProtoMap;
        for( std::map< uint, ProtoMap::TileVec >::iterator it = BR.Orig.begin(); it != BR.Orig.end(); ++it )
        {
            const int tx = (int) ( it->first & 0xFFFF ), ty = (int) ( it->first >> 16 );
            BrClearCell( m, pm, tx, ty );
            const ProtoMap::TileVec& tv = it->second;
            for( size_t i = 0; i < tv.size(); i++ )
                m->HexMngr.SetTile( tv[ i ].NameHash, (ushort) ( tx * 2 ), (ushort) ( ty * 2 ), tv[ i ].OffsX, tv[ i ].OffsY, tv[ i ].Layer, false, false );
        }
        BrSprayRemoveObjs( m );
        for( std::map< uint, ProtoMap::TileVec >::iterator it = BR.SprTileOrig.begin(); it != BR.SprTileOrig.end(); ++it )
        {
            const bool   roof = ( it->first & 0x80000000u ) != 0;
            const ushort hx = (ushort) ( it->first & 0xFFFF ), hy = (ushort) ( ( it->first >> 16 ) & 0x7FFF );
            pm->GetTiles( hx, hy, roof ).clear();
            Field& f = m->HexMngr.GetField( hx, hy );
            ( roof ? f.Roofs : f.Tiles ).clear();
            const ProtoMap::TileVec& tv = it->second;
            for( size_t i = 0; i < tv.size(); i++ )
                m->HexMngr.SetTile( tv[ i ].NameHash, hx, hy, tv[ i ].OffsX, tv[ i ].OffsY, tv[ i ].Layer, roof, false );
        }
        m->HexMngr.RebuildTiles();
    }
    BrEnd();
    m->AddMess( "Brush: cancelled, everything restored." );
}

static void AwClose();                                  // defined in Autowall.inc

static void BrushClose()
{
    if( !BR.Visible )
        return;
    if( BR.Active )
        BrApply( FOMapper::Self );
    BR.Visible = false;
}

static void BrushToggle()
{
    FOMapper* m = FOMapper::Self;
    if( BR.Visible )
    {
        BrushClose();
        return;
    }
    if( !BrMapOk( m ) )
    {
        m->AddMess( "Brush: create or load a map first." );
        return;
    }
    AwClose();
    if( MG.Visible )
        MapGenToggle();                                 // only one floating panel at a time
    BR.HashOk = false;                                  // rescan every time the panel opens, so tiles added later are picked up
    if( !BrInitHashes( m ) )
        return;
    BR.Visible = true;
    BR.Status = "Drag on the map to paint";
}

// ---- layout / drawing --------------------------------------------------------------
static Rect BrushBtn( 1, 266, 77, 292 );

static void BrushPlaceButton()                          // one slot to the left of the MapGen button (absolute screen coords)
{
    MapGenPlaceButton();
    const int step = ( MapGenBtn[ 2 ] - MapGenBtn[ 0 ] ) + 4;
    BrushBtn[ 0 ] = MapGenBtn[ 0 ] - step;
    BrushBtn[ 1 ] = MapGenBtn[ 1 ];
    BrushBtn[ 2 ] = MapGenBtn[ 2 ] - step;
    BrushBtn[ 3 ] = MapGenBtn[ 3 ];
}

static void BrushButtonDraw()
{
    FOMapper* m = FOMapper::Self;
    BrushPlaceButton();
    Rect       r( BrushBtn[ 0 ], BrushBtn[ 1 ], BrushBtn[ 2 ], BrushBtn[ 3 ] );
    const bool hot = m->IsCurInRect( r );
    const uint col = BR.Visible ? RA_ARGB( 255, 130, 255, 130 ) : ( hot ? COLOR_TEXT_DWHITE : COLOR_TEXT_WHITE );
    SprMngr.DrawStr( r, "Brush", FT_NOBREAK | FT_CENTERX | FT_CENTERY, col );
}

static void BrGeom( int& px, int& py )
{
    FOMapper* m = FOMapper::Self;
    BrushPlaceButton();
    px = BrushBtn[ 2 ] - BR_PANEL_W;
    py = BrushBtn[ 1 ] - 6 - BrPanelH();
    if( px < 4 ) px = 4;
    if( py < 8 )  py = 8;
}

static void BrushDraw()
{
    FOMapper* m = FOMapper::Self;
    if( !m->IntVisible )
        return;
    BrushButtonDraw();
    if( !BR.Visible )
        return;
    int px, py;
    BrGeom( px, py );
    const int W = BR_PANEL_W, mid = px + W / 2;
    const BrLay L = BrLayout( py );
    MgFill( px, py, px + W, py + BrPanelH(), RA_ARGB( 235, 22, 17, 11 ) );
    MgFrame( px, py, px + W, py + BrPanelH(), RA_ARGB( 255, 140, 110, 50 ) );
    MgText( px + 8, py + 2, px + W - 8, py + 22, "Brushes", COLOR_TEXT_WHITE, true );

    MgButton( px + 8, L.road, px + W - 8, L.road + 24, BR.Tool == 1 ? "[x] Road" : "Road", true );
    MgButton( px + 8, L.road2, px + W - 8, L.road2 + 24, BR.Tool == 4 ? "[x] Road 2" : "Road 2", BR.SetOk[ 1 ] );
    MgButton( px + 8, L.eraser, px + W - 8, L.eraser + 24, BR.Tool == 2 ? "[x] Eraser" : "Eraser", true );
    MgButton( px + 8, L.spray, px + W - 8, L.spray + 24, BR.Tool == 3 ? "[x] Spray" : "Spray", true );

    char buf[ 32 ];
    if( L.pick >= 0 )
    {
        MgButton( px + 8, L.pick, mid - 3, L.pick + 24, BR.Picking ? "Pick..." : "Pick", true );
        MgButton( mid + 3, L.pick, px + W - 8, L.pick + 24, "Panel", true );
        sprintf( buf, "Spread %d", BR.Spread );
        MgButton( px + 8, L.spread, px + 32, L.spread + 24, "-", true );
        MgText( px + 34, L.spread, px + W - 34, L.spread + 24, buf, COLOR_TEXT_WHITE, true );
        MgButton( px + W - 32, L.spread, px + W - 8, L.spread + 24, "+", true );
    }

    if( BR.Tool == 3 )
        sprintf( buf, "Count %d", BR.SprayCount );
    else
    {
        const int fp = BR.Size + ( BR.Tool == 2 ? 0 : BR_FOOTPRINT_ADD );
        sprintf( buf, "Size %d  (%dx%d)", BR.Size, fp, fp );
    }
    MgButton( px + 8, L.size, px + 32, L.size + 24, "-", true );
    MgText( px + 34, L.size, px + W - 34, L.size + 24, buf, COLOR_TEXT_WHITE, true );
    MgButton( px + W - 32, L.size, px + W - 8, L.size + 24, "+", true );

    MgText( px + 8, L.status, px + W - 8, L.status + 18, BR.Status.c_str(), RA_ARGB( 255, 200, 200, 150 ), true );

    MgButton( px + 8, L.btn, px + 82, L.btn + 24, "Apply", BR.Active );
    MgButton( px + 90, L.btn, px + W - 8, L.btn + 24, "Cancel", BR.Active );
}

// ---- per frame / input -------------------------------------------------------------
static void BrushProcess()
{
    FOMapper* m = FOMapper::Self;
    BrushPlaceButton();
    if( BR.Active && ( !BrMapOk( m ) || BR.Map != m->CurProtoMap ) )
    {
        BrEnd();                                        // map switched under a pending stroke: it stays as it is
        BR.Status = "Map changed - dropped";
        return;
    }
    if( BR.Dirty )
    {
        BR.Dirty = false;
        m->HexMngr.RebuildTiles();                      // once per frame, however many cells changed
    }
}

static bool BrushMouseDown()
{
    FOMapper* m = FOMapper::Self;
    if( !m->IntVisible )
        return false;
    BrushPlaceButton();
    if( MgIn( BrushBtn[ 0 ], BrushBtn[ 1 ], BrushBtn[ 2 ], BrushBtn[ 3 ] ) )
    {
        PlayButtonSound();
        BrushToggle();
        return true;
    }
    if( !BR.Visible )
        return false;

    int px, py;
    BrGeom( px, py );
    if( MgIn( px, py, px + BR_PANEL_W, py + BrPanelH() ) )
    {
        const int W = BR_PANEL_W, mid = px + W / 2;
        const BrLay L = BrLayout( py );
        if( MgIn( px + 8, L.road, px + W - 8, L.road + 24 ) )
        {
            PlayButtonSound();
            BR.Tool = BR.Tool == 1 ? 0 : 1;             // click again = brush off, normal cursor
            BR.Picking = false;
            BR.Status = BR.Tool ? "Drag on the map to paint" : "Brush off";
            return true;
        }
        if( MgIn( px + 8, L.road2, px + W - 8, L.road2 + 24 ) )
        {
            PlayButtonSound();
            if( !BR.SetOk[ 1 ] )
            {
                BR.Status = "Road 2 tiles not found";
                return true;
            }
            BR.Tool = BR.Tool == 4 ? 0 : 4;
            BR.Picking = false;
            BR.Status = BR.Tool ? "Drag on the map to paint" : "Brush off";
            return true;
        }
        if( MgIn( px + 8, L.eraser, px + W - 8, L.eraser + 24 ) )
        {
            PlayButtonSound();
            BR.Tool = BR.Tool == 2 ? 0 : 2;
            BR.Picking = false;
            BR.Status = BR.Tool ? "Drag on the map to erase" : "Brush off";
            return true;
        }
        if( MgIn( px + 8, L.spray, px + W - 8, L.spray + 24 ) )
        {
            PlayButtonSound();
            BR.Tool = BR.Tool == 3 ? 0 : 3;
            BR.Picking = false;
            BR.Status = BR.Tool ? "Pick or select an asset" : "Brush off";
            return true;
        }
        if( L.pick >= 0 )
        {
            if( MgIn( px + 8, L.pick, mid - 3, L.pick + 24 ) )
            {
                PlayButtonSound();
                BR.Picking = !BR.Picking;
                BR.Status = BR.Picking ? "Click an object / tile" : "Pick cancelled";
                return true;
            }
            if( MgIn( mid + 3, L.pick, px + W - 8, L.pick + 24 ) )
            {
                PlayButtonSound();
                BR.Picking = false;
                BrSprayCapture( m );                    // take the asset selected in the asset panel
                return true;
            }
            if( MgIn( px + 8, L.spread, px + 32, L.spread + 24 ) )
            {
                PlayButtonSound();
                if( BR.Spread > 1 ) BR.Spread--;
                return true;
            }
            if( MgIn( px + W - 32, L.spread, px + W - 8, L.spread + 24 ) )
            {
                PlayButtonSound();
                if( BR.Spread < BR_SPREAD_MAX ) BR.Spread++;
                return true;
            }
        }
        if( MgIn( px + 8, L.size, px + 32, L.size + 24 ) )
        {
            PlayButtonSound();
            if( BR.Tool == 3 ) { if( BR.SprayCount > 1 ) BR.SprayCount--; }
            else if( BR.Size > 1 ) BR.Size--;
            return true;
        }
        if( MgIn( px + W - 32, L.size, px + W - 8, L.size + 24 ) )
        {
            PlayButtonSound();
            if( BR.Tool == 3 ) { if( BR.SprayCount < BR_SPRAY_MAX ) BR.SprayCount++; }
            else if( BR.Size < BR_MAX_SIZE ) BR.Size++;
            return true;
        }
        if( BR.Active && MgIn( px + 8, L.btn, px + 82, L.btn + 24 ) )
        {
            PlayButtonSound();
            BrApply( m );
            BR.Status = "Applied";
            return true;
        }
        if( BR.Active && MgIn( px + 90, L.btn, px + W - 8, L.btn + 24 ) )
        {
            PlayButtonSound();
            BrCancel( m );
            BR.Status = "Cancelled";
            return true;
        }
        return true;
    }

    // paint on the map (not through other windows)
    if( !BR.Tool || !BrMapOk( m ) || Keyb::AltDwn || Keyb::ShiftDwn || Keyb::CtrlDwn )
        return false;
    if( m->IntVisible && m->IsCurInRect( m->IntWMain, m->IntX, m->IntY ) )
        return false;
    if( m->ObjVisible && !m->SelectedObj.empty() && m->IsCurInRect( m->ObjWMain, m->ObjX, m->ObjY ) )
        return false;
    if( MG.Active )
        return false;
    int tx, ty;
    if( !BrCursorCell( m, tx, ty ) )
        return false;
    if( BR.Tool == 3 && BR.Picking )
    {
        m->PipCursorObj();                              // the mapper's own Alt+click pipette: selects the asset in the panel
        BR.Picking = false;
        BrSprayCapture( m );
        return true;
    }
    if( !BR.Active )
        BrStart( m );
    BR.Dragging = true;
    if( BR.Tool == 3 )
    {
        BR.LastDabX = BR.LastDabY = -1;
        BrSprayDab( m );
        return true;
    }
    if( BR.Tool == 1 || BR.Tool == 4 )
        BrUseSet( BR.Tool == 4 ? 1 : 0 );               // Road / Road 2
    BR.LastTx = BR.LastTy = -1;
    BrStroke( m, tx, ty );
    return true;
}

static bool BrushMouseMove()
{
    if( !BR.Dragging )
        return false;
    FOMapper* m = FOMapper::Self;
    if( BR.Tool == 3 )
    {
        ushort hx, hy;
        if( BrMapOk( m ) && m->HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, hx, hy ) )
        {
            const int step = BR.Spread / 2 > 2 ? BR.Spread / 2 : 2;
            if( BR.LastDabX < 0 || abs( (int) hx - BR.LastDabX ) + abs( (int) hy - BR.LastDabY ) >= step )
                BrSprayDab( m );                        // a new dab every few hexes of movement
        }
        return true;
    }
    int tx, ty;
    if( BrMapOk( m ) && BrCursorCell( m, tx, ty ) && ( tx != BR.LastTx || ty != BR.LastTy ) )
        BrStroke( m, tx, ty );
    return true;
}

static void BrushMouseUp()
{
    BR.Dragging = false;
    BR.LastDabX = BR.LastDabY = -1;
    BR.LastTx = BR.LastTy = -1;
}
