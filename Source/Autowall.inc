// ============================================================================
// Autowall.inc - wall drawing for the Mapper (Autowall button + panel).
// Include once from Mapper.cpp, directly AFTER   #include "Brush.inc"
//
// Panel:  [<] set name [>]   [Draw area]   Apply / Cancel
//  * Drag an AREA on the map (same hex rectangle as drag-select): all 4 walls go around its edge at once.
//    A too small area (under 3x4 hexes) draws nothing - there is no single-wall mode.
//  * Walls appear while you drag.  Apply = ONE undo step.  Cancel removes them.
//  * Every area drawn in one session is re-resolved together, so areas that touch share walls and
//    get T-junctions / crosses.
//
// Rules (measured on your template maps, Redding, vault and prison):
//  * Vertex hex with two arms at a right angle: corner post   R+D=N  L+D=W  R+U=E  L+U=S
//  * 3 arms (T) and 4 arms (cross): N post (LUD = S, LRU = E)
//  * Free end of a wall: one post (R=N, L=S, U=E, D=W)
//  * Straight pieces alternate through the set's list, one per hex; the hex right after
//    a vertical run's start is left empty (the post covers it); hexes taken by posts are skipped.
//  * Posts may sit offset from the vertex (per set).
// Existing walls on the map are NOT merged yet (only walls drawn in this session).
// ============================================================================

struct AwSet
{
    const char* Name;
    ushort      N, W, E, S;              // corner posts
    int         WdX, WdY, EdX, EdY;      // post offsets for W and E
    int         nTop;    ushort Top[ 4 ];     // horizontal, upper row
    int         nBot;    ushort Bot[ 4 ];     // horizontal, lower row
    int         nLeft;   ushort Left[ 4 ];    // vertical, left column
    int         nRight;  ushort Right[ 4 ];   // vertical, right column
};

// piece ids come from wall.fopro; layout measured on the template buildings
static const AwSet AW_SETS[] =
{
    { "Brick (red)",    5593, 5561, 5560, 5543, 0, 1, 0, 0,  2, { 5591, 5592 },                   2, { 5546, 5551 },                   1, { 5595 },                   2, { 5554, 5555 } },
    { "Stone (black)",  5593, 5538, 5506, 5534, 0, 1, 0, 0,  2, { 5591, 5592 },                   2, { 5505, 5504 },                   1, { 5595 },                   1, { 5508 } },
    { "Wooden shack",   5138, 5367, 5139, 5153, 0, 0, 0, 0,  2, { 5129, 5130 },                   2, { 5097, 5096 },                   1, { 5120 },                   2, { 5114, 5115 } },
    { "Bunker",         5764, 5782, 5784, 5783, 0, 0, 0, -1, 2, { 5752, 5753 },                   2, { 5752, 5753 },                   1, { 5766 },                   1, { 5766 } },
    { "Vault",          5020, 5019, 5021, 5022, 0, 0, 0, 0,  2, { 5001, 5003 },                   2, { 5001, 5003 },                   2, { 5018, 5015 },             2, { 5018, 5015 } },
    { "Fence",          5813, 5823, 5822, 5800, 0, 2, 0, 0,  4, { 5804, 5803, 5801, 5805 },       4, { 5804, 5803, 5801, 5805 },       4, { 5814, 5815, 5816, 5817 }, 4, { 5814, 5815, 5816, 5817 } },
};
static const int AW_SET_NUM = (int) ( sizeof( AW_SETS ) / sizeof( AW_SETS[ 0 ] ) );


// ---- windows and doors: patterns measured on your template buildings -------------------------------
// element kinds: 0 = empty hex, 1 = wall piece, 2 = item (the door object, placed on its own hex).
// A pattern runs along increasing x (N and S sides) or increasing y (E and W sides).
// Sides: 0 N = top row (y = ya), 1 E = right column (x = xb), 2 S = bottom row (y = yb), 3 W = left column (x = xa).
struct AwElem { int kind; ushort pid; };

static const AwElem R_DN[] = { { 1, 5583 }, { 2, 2332 }, { 1, 5588 } };
static const AwElem R_DS[] = { { 1, 5661 }, { 2, 2260 }, { 1, 5660 } };
static const AwElem R_DW[] = { { 1, 5599 }, { 2, 2849 }, { 1, 5600 } };
static const AwElem R_WN[] = { { 1, 5588 }, { 1, 5586 }, { 1, 5588 } };
static const AwElem R_WE[] = { { 1, 5558 }, { 1, 5557 }, { 1, 5556 } };
static const AwElem R_WS[] = { { 1, 5550 }, { 1, 5549 }, { 1, 5548 } };
static const AwElem R_WW[] = { { 1, 5594 }, { 1, 5595 }, { 1, 5596 }, { 1, 5597 } };

static const AwElem B_DN[] = { { 1, 5494 }, { 2, 2260 }, { 1, 5493 } };
static const AwElem B_DS[] = { { 1, 5490 }, { 2, 2258 }, { 1, 5489 } };
static const AwElem B_WN[] = { { 1, 5589 } };
static const AwElem B_WS[] = { { 1, 5503 } };

static const AwElem W_DS[] = { { 1, 5105 }, { 2, 2406 }, { 1, 5104 } };
static const AwElem W_DE[] = { { 1, 5744 }, { 2, 2482 }, { 1, 5745 } };
static const AwElem W_WN[] = { { 1, 5124 }, { 1, 5123 }, { 1, 5122 } };
static const AwElem W_WW[] = { { 1, 5250 } };

static const AwElem U_DNS[] = { { 1, 5792 }, { 0, 0 }, { 2, 2152 }, { 0, 0 }, { 1, 5791 } };
static const AwElem U_DEW[] = { { 1, 5798 }, { 2, 2154 }, { 0, 0 }, { 1, 5797 } };

struct AwFeat { const AwElem* Door[ 4 ]; int DoorN[ 4 ]; const AwElem* Win[ 4 ]; int WinN[ 4 ]; int DoorP[ 4 ]; int WinP[ 4 ]; };
// DoorP / WinP: parity (0 even, 1 odd) the pattern must start on, measured in the real maps; -1 = any
// same order as AW_SETS:  Brick, Stone, Wooden, Bunker, Vault, Fence   (arrays: N, E, S, W)
static const AwFeat AW_FEATS[] =
{
    { { R_DN, 0, R_DS, R_DW },       { 3, 0, 3, 3 },    { R_WN, R_WE, R_WS, R_WW }, { 3, 3, 3, 4 }, { 1, -1, 1, 0 }, { 1, -1, 0, 1 } },
    { { B_DN, 0, B_DS, R_DW },       { 3, 0, 3, 3 },    { B_WN, 0, B_WS, R_WW },    { 1, 0, 1, 4 }, { 1, -1, 1, 0 }, { 0, -1, 0, 1 } },
    { { 0, W_DE, W_DS, 0 },          { 0, 3, 3, 0 },    { W_WN, 0, 0, W_WW },       { 3, 0, 0, 1 }, { -1, 0, 0, -1 }, { 0, -1, -1, -1 } },
    { { U_DNS, U_DEW, U_DNS, U_DEW }, { 5, 4, 5, 4 },   { 0, 0, 0, 0 },             { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { -1, -1, -1, -1 } },
    { { 0, 0, 0, 0 },                { 0, 0, 0, 0 },    { 0, 0, 0, 0 },             { 0, 0, 0, 0 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
    { { 0, 0, 0, 0 },                { 0, 0, 0, 0 },    { 0, 0, 0, 0 },             { 0, 0, 0, 0 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
};

// 1 = the first hex after a post in a vertical wall also gets a piece (Wooden: otherwise there is a one-hex gap)
static const int AW_FILL1[ 6 ] = { 0, 0, 1, 0, 0, 0 };

struct AwArea { int xa, ya, xb, yb, nWin, nDoor, mask, roof; };   // one drawn area and the options it was drawn with

static uint AwRnd( uint& s )                            // small deterministic generator: the same area always gets the same layout
{
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

#define AW_PANEL_W    ( 190 )
#define AW_PANEL_H    ( 24 + 30 + 30 + 30 + 30 + 30 + 30 + 20 + 30 + 6 )

typedef std::map< uint, uchar > AwMap;                  // key (y<<16|x) -> side (0 top/left, 1 bottom/right)

struct AwState
{
    bool                      Visible;
    bool                      Active;                   // walls of this session are on the map (not applied)
    int                       Mode;                     // 0 off, 1 line, 2 room
    int                       SetIdx;
    bool                      Dragging;
    int                       StartX, StartY, CurX, CurY;
    ProtoMap*                 Map;
    AwMap                     H, V;                     // committed strokes of this session
    std::vector< MapObject* > Objs;                     // objects currently placed by the session
    int                       Missing;                  // pieces whose proto was not loaded
    std::vector< AwArea >     Areas;                    // committed areas of this session
    int                       NWin, NDoor, SideMask;    // options for the next area (SideMask bits: N E S W)
    int                       RoofSel;                  // 0 = no roof
    std::map< uint, ProtoMap::TileVec > RoofOrig;       // roof cells touched in this session -> roof tiles before
    std::map< std::string, uint >       TileMap;        // tile file name (no folder, no .frm) -> hash
    bool                      TileMapOk;
    std::string               Status;

    AwState(): Visible( false ), Active( false ), NWin( 2 ), NDoor( 1 ), SideMask( 15 ), RoofSel( 0 ), TileMapOk( false ), Mode( 2 ), SetIdx( 0 ), Dragging( false ),
               StartX( 0 ), StartY( 0 ), CurX( -1 ), CurY( -1 ), Map( NULL ), Missing( 0 ) {}
};
static AwState AW;

static inline uint AwKey( int x, int y ) { return ( (uint) y << 16 ) | (uint) x; }
static inline bool AwHas( const AwMap& m, int x, int y ) { return x >= 0 && y >= 0 && m.count( AwKey( x, y ) ) != 0; }

// ---- object helpers ----------------------------------------------------------------
static void AwRemoveObjs( FOMapper* m )
{
    ProtoMap* pm = AW.Map ? AW.Map : m->CurProtoMap;
    for( size_t i = 0; i < AW.Objs.size(); i++ )
    {
        MapObject* o = AW.Objs[ i ];
        if( pm )
        {
            std::vector< MapObject* >::iterator it = std::find( pm->MObjects.begin(), pm->MObjects.end(), o );
            if( it != pm->MObjects.end() )
                pm->MObjects.erase( it );
        }
        m->HexMngr.FinishItem( o->RunTime.MapObjId, 0 );
        SAFEREL( o );
    }
    if( !AW.Objs.empty() )
        m->HexMngr.RefreshMap();                        // the Mapper's SelectDelete does this too
    AW.Objs.clear();
}

static void AwPlace( FOMapper* m, ushort pid, int x, int y )
{
    if( x < 0 || y < 0 )
        return;
    MapObject* o = m->ParseProto( pid, (ushort) x, (ushort) y, NULL );
    if( o )
        AW.Objs.push_back( o );
    else
        AW.Missing++;
}

// ---- resolver ----------------------------------------------------------------------
static int AwPostFor( bool L, bool R, bool U, bool D )  // 0 N, 1 W, 2 E, 3 S ; -1 = straight
{
    const int n = ( L ? 1 : 0 ) + ( R ? 1 : 0 ) + ( U ? 1 : 0 ) + ( D ? 1 : 0 );
    if( n == 1 )
    {
        if( R ) return 0;
        if( L ) return 3;
        if( U ) return 2;
        return 1;                       // D
    }
    if( n == 2 )
    {
        if( L && R ) return -1;
        if( U && D ) return -1;
        if( R && D ) return 0;
        if( L && D ) return 1;
        if( R && U ) return 2;
        return 3;                       // L && U
    }
    if( n == 3 )
    {
        if( !R ) return 3;              // L U D
        if( !D ) return 2;              // L R U
        return 0;                       // L R D, R U D
    }
    return 0;                           // cross
}

// ---- roofs ---------------------------------------------------------------------------------------
// Layouts decoded from your maps. The roof grid is 2x2 hexes like the floor; tile columns sit on the same parity as the walls.
static const char* AW_ROOFS[] = { "No roof", "Shingle (tp)", "Planks (crbm)", "Adobe (adb)", "Planks 2 (plk)" };
static const int   AW_ROOF_NUM = 5;

static bool AwTileHash( FOMapper* m, const char* name, uint& hash )
{
    if( !AW.TileMapOk )
    {
        AW.TileMap.clear();
        for( auto it = m->Tabs[ INT_MODE_TILE ].begin(); it != m->Tabs[ INT_MODE_TILE ].end(); ++it )
        {
            FOMapper::SubTab& st = it->second;
            for( size_t i = 0; i < st.TileNames.size() && i < st.TileHashes.size(); i++ )
            {
                std::string n = st.TileNames[ i ];
                for( size_t k = 0; k < n.size(); k++ )
                    if( n[ k ] >= 'A' && n[ k ] <= 'Z' )
                        n[ k ] = (char) ( n[ k ] - 'A' + 'a' );
                const size_t sl = n.find_last_of( "\\/" );
                if( sl != std::string::npos )
                    n = n.substr( sl + 1 );
                if( n.size() > 4 && n.compare( n.size() - 4, 4, ".frm" ) == 0 )
                    n = n.substr( 0, n.size() - 4 );
                if( !AW.TileMap.count( n ) )
                    AW.TileMap[ n ] = st.TileHashes[ i ];
            }
        }
        AW.TileMapOk = true;
    }
    std::map< std::string, uint >::const_iterator f = AW.TileMap.find( name );
    if( f == AW.TileMap.end() )
        return false;
    hash = f->second;
    return true;
}

// name of the roof tile for column c, row r of a W x H roof ("" = no tile there)
static std::string AwRoofName( int roof, int c, int r, int W, int H, int x, int y )
{
    char b[ 24 ];
    const uint h = ( (uint) x * 73856093u ) ^ ( (uint) y * 19349663u );
    if( roof == 1 )                                      // tp: eave ring (no corners) + shaded inner ring + centre
    {
        if( W < 4 || H < 4 )
            return "";
        static const int T[ 4 ] = { 23, 20, 19, 16 }, B[ 4 ] = { 21, 17, 18, 21 }, L[ 3 ] = { 11, 12, 13 }, R[ 3 ] = { 31, 32, 33 };
        int n = 0;
        const bool cornerCell = ( c == 0 || c == W - 1 ) && ( r == 0 || r == H - 1 );
        if( cornerCell ) return "";
        if( r == 0 ) n = T[ ( c - 1 ) % 4 ];
        else if( r == H - 1 ) n = B[ ( c - 1 ) % 4 ];
        else if( c == 0 ) n = L[ ( r - 1 ) % 3 ];
        else if( c == W - 1 ) n = R[ ( r - 1 ) % 3 ];
        else if( c == 1 && r == 1 ) n = 161;
        else if( c == W - 2 && r == 1 ) n = 164;
        else if( c == 1 && r == H - 2 ) n = 162;
        else if( c == W - 2 && r == H - 2 ) n = 163;
        else if( r == 1 ) n = 157;
        else if( r == H - 2 ) n = 159;
        else if( c == 1 ) n = 158;
        else if( c == W - 2 ) n = 160;
        else n = ( ( c - 2 ) % 2 == 0 ) ? 156 : 155;
        sprintf( b, "tp%03d", n );
    }
    else if( roof == 2 )                                 // crbm: two tiles alternating along x
        sprintf( b, "crbm%03d", ( c % 2 == 0 ) ? 7 : 4 );
    else if( roof == 3 )                                 // adb: eight variants at random
        sprintf( b, "adb%03d", 28 + (int) ( ( h >> 4 ) % 8 ) );
    else if( roof == 4 )                                 // plk: 2000, with the odd 1000
        sprintf( b, "plk%04d", ( ( h >> 4 ) % 8 ) == 0 ? 1000 : 2000 );
    else
        return "";
    return b;
}

static void AwRoofRestore( FOMapper* m )
{
    ProtoMap* pm = AW.Map ? AW.Map : m->CurProtoMap;
    if( !pm || AW.RoofOrig.empty() )
        return;
    for( std::map< uint, ProtoMap::TileVec >::iterator it = AW.RoofOrig.begin(); it != AW.RoofOrig.end(); ++it )
    {
        const ushort hx = (ushort) ( it->first & 0xFFFF ), hy = (ushort) ( it->first >> 16 );
        pm->GetTiles( hx, hy, true ).clear();
        m->HexMngr.GetField( hx, hy ).Roofs.clear();
        const ProtoMap::TileVec& tv = it->second;
        for( size_t i = 0; i < tv.size(); i++ )
            m->HexMngr.SetTile( tv[ i ].NameHash, hx, hy, tv[ i ].OffsX, tv[ i ].OffsY, tv[ i ].Layer, true, false );
    }
    AW.RoofOrig.clear();
    m->HexMngr.RebuildTiles();
}

static void AwRoofs( FOMapper* m, const std::vector< AwArea >& areas )
{
    ProtoMap* pm = m->CurProtoMap;
    std::string missing;
    bool        any = false;
    for( size_t i = 0; i < areas.size(); i++ )
    {
        const AwArea& a = areas[ i ];
        if( a.roof <= 0 )
            continue;
        const bool over = ( a.roof == 1 );                // the shingle roof overhangs the walls by one tile
        const int  cx0 = over ? a.xa - 2 : a.xa, cx1 = over ? a.xb : a.xb - 2;
        int        ry0 = over ? a.ya - 1 : a.ya + 1, ry1 = over ? a.yb + 1 : a.yb - 1;
        if( ry0 & 1 ) ry0++;
        if( ry1 & 1 ) ry1--;
        const int W = ( cx1 - cx0 ) / 2 + 1, H = ( ry1 - ry0 ) / 2 + 1;
        if( W < 1 || H < 1 )
            continue;
        for( int r = 0; r < H; r++ )
            for( int c = 0; c < W; c++ )
            {
                const int         x = cx0 + c * 2, y = ry0 + r * 2;
                const std::string name = AwRoofName( a.roof, c, r, W, H, x, y );
                if( name.empty() || x < 0 || y < 0 )
                    continue;
                uint hash;
                if( !AwTileHash( m, name.c_str(), hash ) )
                {
                    if( missing.find( name ) == std::string::npos )
                        missing += ( missing.empty() ? "" : ", " ) + name;
                    continue;
                }
                const uint key = ( (uint) y << 16 ) | (uint) x;
                if( !AW.RoofOrig.count( key ) )
                    AW.RoofOrig[ key ] = pm->GetTiles( (ushort) x, (ushort) y, true );
                pm->GetTiles( (ushort) x, (ushort) y, true ).clear();
                m->HexMngr.GetField( (ushort) x, (ushort) y ).Roofs.clear();
                m->ParseTile( hash, (ushort) x, (ushort) y, 0, 0, 0, true );
                any = true;
            }
    }
    if( any )
        m->HexMngr.RebuildTiles();
    if( !missing.empty() )
        AW.Status = "Roof tiles missing: " + missing;
}

struct AwElemPut { int x, y; AwElem e; };

// windows and doors of one area: random spots on the chosen sides, never overlapping, never next to a corner
static void AwFeatures( const AwArea& a, std::vector< AwElemPut >& out, std::set< uint >& ovr )
{
    const AwFeat& F = AW_FEATS[ AW.SetIdx ];
    uint seed = ( (uint) a.xa * 73856093u ) ^ ( (uint) a.ya * 19349663u ) ^ ( (uint) a.xb * 83492791u ) ^ ( (uint) a.yb * 2654435761u ) ^ 0x9E3779B9u;
    std::set< uint > taken;
    for( int pass = 0; pass < 2; pass++ )               // doors first, then windows
    {
        const int want = pass == 0 ? a.nDoor : a.nWin;
        for( int n = 0; n < want; n++ )
            for( int tries = 0; tries < 40; tries++ )
            {
                const int side = (int) ( AwRnd( seed ) % 4 );
                const int ui = side == 1 ? 3 : ( side == 3 ? 1 : side );      // the E / W buttons are the other way round than my data sides
                if( !( a.mask & ( 1 << ui ) ) )
                    continue;
                const AwElem* pat = pass == 0 ? F.Door[ side ] : F.Win[ side ];
                const int     len = pass == 0 ? F.DoorN[ side ] : F.WinN[ side ];
                if( !pat || len <= 0 )
                    continue;
                const bool horiz = ( side == 0 || side == 2 );
                const int  lo = horiz ? a.xa + 2 : a.ya + 3, hi = horiz ? a.xb - 1 - len : a.yb - 1 - len;
                if( hi < lo )
                    continue;
                int       start = lo + (int) ( AwRnd( seed ) % (uint) ( hi - lo + 1 ) );
                const int par = pass == 0 ? F.DoorP[ side ] : F.WinP[ side ];
                if( par >= 0 && ( start & 1 ) != par )          // the pieces only line up on their own hex parity
                    start += ( start + 1 <= hi ) ? 1 : -1;
                if( start < lo || start > hi || ( par >= 0 && ( start & 1 ) != par ) )
                    continue;
                const int fixed = side == 0 ? a.ya : ( side == 2 ? a.yb : ( side == 1 ? a.xb : a.xa ) );
                bool      ok = true;
                for( int k = -1; k <= len && ok; k++ )
                    if( taken.count( horiz ? AwKey( start + k, fixed ) : AwKey( fixed, start + k ) ) )
                        ok = false;
                if( !ok )
                    continue;
                for( int k = 0; k < len; k++ )
                {
                    AwElemPut p;
                    p.x = horiz ? start + k : fixed;
                    p.y = horiz ? fixed : start + k;
                    p.e = pat[ k ];
                    taken.insert( AwKey( p.x, p.y ) );
                    ovr.insert( AwKey( p.x, p.y ) );
                    out.push_back( p );
                }
                break;
            }
    }
}

struct AwStraight { int x, y; bool horiz; uchar side; int idx; };

static void AwBuild( FOMapper* m, const AwMap& H, const AwMap& V, const std::vector< AwArea >& areas )
{
    const AwSet& S = AW_SETS[ AW.SetIdx ];
    AwRemoveObjs( m );
    AwRoofRestore( m );
    AW.Missing = 0;

    std::set< uint > all, used;
    for( AwMap::const_iterator it = H.begin(); it != H.end(); ++it )
        all.insert( it->first );
    for( AwMap::const_iterator it = V.begin(); it != V.end(); ++it )
        all.insert( it->first );

    std::vector< AwStraight > st;
    for( std::set< uint >::const_iterator it = all.begin(); it != all.end(); ++it )
    {
        const int  x = (int) ( *it & 0xFFFF ), y = (int) ( *it >> 16 );
        const bool inH = H.count( *it ) != 0, inV = V.count( *it ) != 0;
        const bool L = inH && AwHas( H, x - 1, y ), R = inH && AwHas( H, x + 1, y );
        const bool U = inV && AwHas( V, x, y - 1 ), D = inV && AwHas( V, x, y + 1 );
        const int  n = ( L ? 1 : 0 ) + ( R ? 1 : 0 ) + ( U ? 1 : 0 ) + ( D ? 1 : 0 );

        int post = -2;                                  // -2 = isolated hex
        if( n > 0 )
            post = AwPostFor( L, R, U, D );
        if( post >= 0 )
        {
            ushort pid = S.N;
            int    px = x, py = y;
            if( post == 1 ) { pid = S.W; px += S.WdX; py += S.WdY; }
            else if( post == 2 ) { pid = S.E; px += S.EdX; py += S.EdY; }
            else if( post == 3 ) pid = S.S;
            AwPlace( m, pid, px, py );
            used.insert( AwKey( px, py ) );
            continue;
        }
        // straight piece (or an isolated hex)
        if( inH && ( !inV || L || R || n == 0 ) )
        {
            int xs = x;
            while( AwHas( H, xs - 1, y ) )
                xs--;
            AwStraight s = { x, y, true, H.find( *it )->second, x - xs };
            if( s.idx < 1 ) s.idx = 1;
            st.push_back( s );
        }
        else if( inV )
        {
            int ys = y;
            while( AwHas( V, x, ys - 1 ) )
                ys--;
            if( y - ys == 1 && !AW_FILL1[ AW.SetIdx ] )
                continue;                               // covered by the start post
            AwStraight s = { x, y, false, V.find( *it )->second, y - ys };
            if( s.idx < 1 ) s.idx = 1;
            st.push_back( s );
        }
    }
    std::vector< AwElemPut > feat;
    std::set< uint >         ovr;
    for( size_t i = 0; i < areas.size(); i++ )
        AwFeatures( areas[ i ], feat, ovr );
    for( size_t i = 0; i < st.size(); i++ )
    {
        const AwStraight& s = st[ i ];
        if( used.count( AwKey( s.x, s.y ) ) || ovr.count( AwKey( s.x, s.y ) ) )
            continue;
        const ushort* arr;
        int           len;
        if( s.horiz ) { arr = s.side ? S.Bot : S.Top;    len = s.side ? S.nBot : S.nTop; }
        else          { arr = s.side ? S.Right : S.Left; len = s.side ? S.nRight : S.nLeft; }
        // rows follow the hex parity (the long / short pieces sit on fixed odd / even columns); columns just walk the list
        const int phase = AW.SetIdx == 4 ? 2 : 3;               // Vault has its long piece on even x, the other sets on odd x
        const int pick = s.horiz ? ( ( s.x + phase ) % len ) : ( ( s.idx - 1 ) % len );
        AwPlace( m, arr[ pick ], s.x, s.y );
    }
    for( size_t i = 0; i < feat.size(); i++ )           // window pieces and doors (kind 0 = a deliberately empty hex)
        if( feat[ i ].e.kind != 0 )
            AwPlace( m, feat[ i ].e.pid, feat[ i ].x, feat[ i ].y );
    AwRoofs( m, areas );
}

// ---- strokes -----------------------------------------------------------------------
static bool AwAddStroke( int /*mode*/, int x0, int y0, int x1, int y1, AwMap& H, AwMap& V, std::vector< AwArea >* areas )
{
    const int xa = std::min( x0, x1 ) & ~1, xb = std::max( x0, x1 ) & ~1, ya = std::min( y0, y1 ), yb = std::max( y0, y1 );   // even columns, like the real buildings
    if( xb - xa < 2 || yb - ya < 3 )
        return false;
    for( int x = xa; x <= xb; x++ )
    {
        H[ AwKey( x, ya ) ] = 0;
        H[ AwKey( x, yb ) ] = 1;
    }
    for( int y = ya; y <= yb; y++ )
    {
        V[ AwKey( xa, y ) ] = 0;
        V[ AwKey( xb, y ) ] = 1;
    }
    if( areas )
    {
        const AwArea ar = { xa, ya, xb, yb, AW.NWin, AW.NDoor, AW.SideMask, AW.RoofSel };
        areas->push_back( ar );
    }
    return true;
}

static bool AwMapOk( FOMapper* m ) { return m->CurProtoMap && m->HexMngr.IsMapLoaded(); }

static bool AwCursorHex( FOMapper* m, int& hx, int& hy )
{
    ushort ux, uy;
    if( !m->HexMngr.GetHexPixel( GameOpt.MouseX, GameOpt.MouseY, ux, uy ) )
        return false;
    hx = ux;
    hy = uy;
    return true;
}

static void AwPreview( FOMapper* m )
{
    AwMap H( AW.H ), V( AW.V );
    std::vector< AwArea > areas( AW.Areas );
    if( !AwAddStroke( AW.Mode, AW.StartX, AW.StartY, AW.CurX, AW.CurY, H, V, &areas ) )
        AW.Status = "Area too small (min 3x4)";
    else
        AW.Status = "Release to keep the area";
    AwBuild( m, H, V, areas );
}

// ---- session -----------------------------------------------------------------------
static void AwStart( FOMapper* m )
{
    ProtoMap*    pmap = m->CurProtoMap;
    UndoHistory* hist = UndoFind( pmap );
    if( !hist )
        UndoCreate( pmap );
    else if( UndoDirty )
        UndoCommit( m, pmap, hist );                    // undo baseline BEFORE the first change
    m->SelectClear();
    AW.Map = pmap;
    AW.H.clear();
    AW.V.clear();
    AW.Areas.clear();
    AW.Active = true;
    MapGenPreviewActive = true;
}

static void AwEnd()
{
    AW.Active = false;
    AW.Dragging = false;
    AW.Map = NULL;
    AW.Objs.clear();
    AW.H.clear();
    AW.V.clear();
    AW.Areas.clear();
    AW.RoofOrig.clear();
    MapGenPreviewActive = false;
}

static void AwApply( FOMapper* m )
{
    if( !AW.Active )
        return;
    const size_t n = AW.Objs.size();
    AwEnd();
    UndoMarkDirty();                                    // one undo step for the whole session
    m->AddMessFormat( "Autowall: applied (%d pieces). Ctrl+Z undoes it.", (int) n );
}

static void AwCancel( FOMapper* m )
{
    if( !AW.Active )
        return;
    if( AwMapOk( m ) && AW.Map == m->CurProtoMap )
    {
        AwRemoveObjs( m );
        AwRoofRestore( m );
    }
    AwEnd();
    m->AddMess( "Autowall: cancelled." );
}

static void AwClose()
{
    if( !AW.Visible )
        return;
    if( AW.Active )
        AwApply( FOMapper::Self );
    AW.Visible = false;
}

static void AutowallToggle()
{
    FOMapper* m = FOMapper::Self;
    if( AW.Visible )
    {
        AwClose();
        return;
    }
    if( !AwMapOk( m ) )
    {
        m->AddMess( "Autowall: create or load a map first." );
        return;
    }
    BrushClose();
    if( MG.Visible )
        MapGenToggle();
    AW.Visible = true;
    AW.Mode = 2;
    AW.Status = "Drag an area on the map";
}

// ---- layout / drawing --------------------------------------------------------------
static Rect AwBtn( 1, 299, 77, 325 );

static void AwPlaceButton()                             // one slot to the left of the Brush button (absolute screen coords)
{
    BrushPlaceButton();
    const int step = ( BrushBtn[ 2 ] - BrushBtn[ 0 ] ) + 4;
    AwBtn[ 0 ] = BrushBtn[ 0 ] - step;
    AwBtn[ 1 ] = BrushBtn[ 1 ];
    AwBtn[ 2 ] = BrushBtn[ 2 ] - step;
    AwBtn[ 3 ] = BrushBtn[ 3 ];
}

static void AutowallButtonDraw()
{
    FOMapper* m = FOMapper::Self;
    AwPlaceButton();
    Rect       r( AwBtn[ 0 ], AwBtn[ 1 ], AwBtn[ 2 ], AwBtn[ 3 ] );
    const bool hot = m->IsCurInRect( r );
    const uint col = AW.Visible ? RA_ARGB( 255, 130, 255, 130 ) : ( hot ? COLOR_TEXT_DWHITE : COLOR_TEXT_WHITE );
    SprMngr.DrawStr( r, "Autowall", FT_NOBREAK | FT_CENTERX | FT_CENTERY, col );
}

static void AwGeom( int& px, int& py )
{
    FOMapper* m = FOMapper::Self;
    AwPlaceButton();
    px = AwBtn[ 2 ] - AW_PANEL_W;
    py = AwBtn[ 1 ] - 6 - AW_PANEL_H;
    if( px < 4 ) px = 4;
    if( py < 8 ) py = 8;
}

static void AutowallDraw()
{
    FOMapper* m = FOMapper::Self;
    if( !m->IntVisible )
        return;
    AutowallButtonDraw();
    if( !AW.Visible )
        return;
    int px, py;
    AwGeom( px, py );
    const int W = AW_PANEL_W;
    MgFill( px, py, px + W, py + AW_PANEL_H, RA_ARGB( 235, 22, 17, 11 ) );
    MgFrame( px, py, px + W, py + AW_PANEL_H, RA_ARGB( 255, 140, 110, 50 ) );
    MgText( px + 8, py + 2, px + W - 8, py + 22, "Autowall", COLOR_TEXT_WHITE, true );

    int y = py + 24;
    MgButton( px + 8, y, px + 32, y + 24, "<", true );
    MgText( px + 34, y, px + W - 34, y + 24, AW_SETS[ AW.SetIdx ].Name, COLOR_TEXT_WHITE, true );
    MgButton( px + W - 32, y, px + W - 8, y + 24, ">", true );
    y += 30;

    const int mid = px + W / 2;
    MgButton( px + 8, y, px + W - 8, y + 24, AW.Mode ? "[x] Draw area" : "Draw area", true );
    y += 30;

    char buf[ 32 ];
    sprintf( buf, "Windows %d", AW.NWin );
    MgButton( px + 8, y, px + 32, y + 24, "-", true );
    MgText( px + 34, y, px + W - 34, y + 24, buf, COLOR_TEXT_WHITE, true );
    MgButton( px + W - 32, y, px + W - 8, y + 24, "+", true );
    y += 30;
    sprintf( buf, "Doors %d", AW.NDoor );
    MgButton( px + 8, y, px + 32, y + 24, "-", true );
    MgText( px + 34, y, px + W - 34, y + 24, buf, COLOR_TEXT_WHITE, true );
    MgButton( px + W - 32, y, px + W - 8, y + 24, "+", true );
    y += 30;
    {
        static const char* on[ 4 ] = { "[N]", "[E]", "[S]", "[W]" };
        static const char* off[ 4 ] = { "N", "E", "S", "W" };
        for( int i = 0; i < 4; i++ )
            MgButton( px + 8 + i * 44, y, px + 8 + i * 44 + 40, y + 24, ( AW.SideMask & ( 1 << i ) ) ? on[ i ] : off[ i ], true );
    }
    y += 30;

    MgButton( px + 8, y, px + 32, y + 24, "<", true );
    MgText( px + 34, y, px + W - 34, y + 24, AW_ROOFS[ AW.RoofSel ], COLOR_TEXT_WHITE, true );
    MgButton( px + W - 32, y, px + W - 8, y + 24, ">", true );
    y += 30;
    MgText( px + 8, y, px + W - 8, y + 18, AW.Status.c_str(), RA_ARGB( 255, 200, 200, 150 ), true );
    y += 20;

    MgButton( px + 8, y, mid - 3, y + 24, "Apply", AW.Active );
    MgButton( mid + 3, y, px + W - 8, y + 24, "Cancel", AW.Active );
}

// ---- per frame / input -------------------------------------------------------------
static void AutowallProcess()
{
    FOMapper* m = FOMapper::Self;
    AwPlaceButton();
    if( AW.Active && ( !AwMapOk( m ) || AW.Map != m->CurProtoMap ) )
    {
        AwEnd();                                        // map switched under a pending session
        AW.Status = "Map changed - dropped";
    }
}

static bool AutowallMouseDown()
{
    FOMapper* m = FOMapper::Self;
    if( !m->IntVisible )
        return false;
    AwPlaceButton();
    if( MgIn( AwBtn[ 0 ], AwBtn[ 1 ], AwBtn[ 2 ], AwBtn[ 3 ] ) )
    {
        PlayButtonSound();
        AutowallToggle();
        return true;
    }
    if( !AW.Visible )
        return false;

    int px, py;
    AwGeom( px, py );
    const int W = AW_PANEL_W, mid = px + W / 2;
    if( MgIn( px, py, px + W, py + AW_PANEL_H ) )
    {
        int y = py + 24;
        if( MgIn( px + 8, y, px + 32, y + 24 ) )
        {
            PlayButtonSound();
            AW.SetIdx = ( AW.SetIdx + AW_SET_NUM - 1 ) % AW_SET_NUM;
            if( AW.Active )
                AwBuild( m, AW.H, AW.V, AW.Areas );
            return true;
        }
        if( MgIn( px + W - 32, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            AW.SetIdx = ( AW.SetIdx + 1 ) % AW_SET_NUM;
            if( AW.Active )
                AwBuild( m, AW.H, AW.V, AW.Areas );
            return true;
        }
        y += 30;
        if( MgIn( px + 8, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            AW.Mode = AW.Mode ? 0 : 2;
            AW.Status = AW.Mode ? "Drag an area on the map" : "Autowall off";
            return true;
        }
        y += 30;
        if( MgIn( px + 8, y, px + 32, y + 24 ) )
        {
            PlayButtonSound();
            if( AW.NWin > 0 ) AW.NWin--;
            return true;
        }
        if( MgIn( px + W - 32, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            if( AW.NWin < 8 ) AW.NWin++;
            return true;
        }
        y += 30;
        if( MgIn( px + 8, y, px + 32, y + 24 ) )
        {
            PlayButtonSound();
            if( AW.NDoor > 0 ) AW.NDoor--;
            return true;
        }
        if( MgIn( px + W - 32, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            if( AW.NDoor < 4 ) AW.NDoor++;
            return true;
        }
        y += 30;
        for( int i = 0; i < 4; i++ )
            if( MgIn( px + 8 + i * 44, y, px + 8 + i * 44 + 40, y + 24 ) )
            {
                PlayButtonSound();
                AW.SideMask ^= ( 1 << i );
                return true;
            }
        y += 30;
        if( MgIn( px + 8, y, px + 32, y + 24 ) )
        {
            PlayButtonSound();
            AW.RoofSel = ( AW.RoofSel + AW_ROOF_NUM - 1 ) % AW_ROOF_NUM;
            return true;
        }
        if( MgIn( px + W - 32, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            AW.RoofSel = ( AW.RoofSel + 1 ) % AW_ROOF_NUM;
            return true;
        }
        y += 50;
        if( AW.Active && MgIn( px + 8, y, mid - 3, y + 24 ) )
        {
            PlayButtonSound();
            AwApply( m );
            AW.Status = "Applied";
            return true;
        }
        if( AW.Active && MgIn( mid + 3, y, px + W - 8, y + 24 ) )
        {
            PlayButtonSound();
            AwCancel( m );
            AW.Status = "Cancelled";
            return true;
        }
        return true;
    }

    // drawing on the map (not through other windows)
    if( !AW.Mode || !AwMapOk( m ) || Keyb::AltDwn || Keyb::ShiftDwn || Keyb::CtrlDwn )
        return false;
    if( m->IntVisible && m->IsCurInRect( m->IntWMain, m->IntX, m->IntY ) )
        return false;
    if( m->ObjVisible && !m->SelectedObj.empty() && m->IsCurInRect( m->ObjWMain, m->ObjX, m->ObjY ) )
        return false;
    if( MG.Active || BR.Active )
        return false;
    int hx, hy;
    if( !AwCursorHex( m, hx, hy ) )
        return false;
    if( !AW.Active )
        AwStart( m );
    AW.Dragging = true;
    AW.StartX = AW.CurX = hx;
    AW.StartY = AW.CurY = hy;
    AwPreview( m );
    return true;
}

static bool AutowallMouseMove()
{
    if( !AW.Dragging )
        return false;
    FOMapper* m = FOMapper::Self;
    int       hx, hy;
    if( AwMapOk( m ) && AwCursorHex( m, hx, hy ) && ( hx != AW.CurX || hy != AW.CurY ) )
    {
        AW.CurX = hx;
        AW.CurY = hy;
        AwPreview( m );
    }
    return true;
}

static void AutowallMouseUp()
{
    if( !AW.Dragging )
        return;
    AW.Dragging = false;
    FOMapper* m = FOMapper::Self;
    if( !AwMapOk( m ) )
        return;
    int hx, hy;
    if( AwCursorHex( m, hx, hy ) && ( hx != AW.CurX || hy != AW.CurY ) )
    {
        AW.CurX = hx;
        AW.CurY = hy;
        AwPreview( m );
    }
    if( AwAddStroke( AW.Mode, AW.StartX, AW.StartY, AW.CurX, AW.CurY, AW.H, AW.V, &AW.Areas ) )
    {
        char buf[ 64 ];
        if( AW.Missing )
            sprintf( buf, "%d pieces missing (proto not loaded)", AW.Missing );
        else
            sprintf( buf, "%d pieces - Apply or draw more", (int) AW.Objs.size() );
        AW.Status = buf;
    }
    AwBuild( m, AW.H, AW.V, AW.Areas );                 // final state (a too-small room leaves no trace)
}
