#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <math.h>
#include <assert.h>
//#include <tr1/unordered_set>
#include <unordered_set>
#include <time.h>

#include "netcdf_utils.h"
#include "pnt.h"
#include "edge.h"

#define MESH_SPEC 1.0
#define ID_LEN 40

using namespace std;
//using namespace tr1;

int nCells, nVertices, vertex_degree;
int maxEdges;
bool spherical;
double xCellRange[2];
double yCellRange[2];
double zCellRange[2];
double xVertexRange[2];
double yVertexRange[2];
double zVertexRange[2];
double xCellDistance, yCellDistance, zCellDistance;
double xVertexDistance, yVertexDistance, zVertexDistance;
double xPeriodicFix, yPeriodicFix;
string in_history = "";
string in_mesh_id = "";
string in_parent_id = "";

// Connectivity and location information {{{

vector<pnt> cells;
vector<pnt> edges;
vector<pnt> vertices;
vector< vector<int> > nEdgesOnCell;
vector< vector<int> > cellsOnEdge;
vector< vector<int> > verticesOnEdge;
vector< vector<int> > edgesOnVertex;
vector< vector<int> > cellsOnVertex;
vector< vector<int> > cellsOnCell;
vector< vector<int> > edgesOnCell;
vector< vector<int> > verticesOnCell;
vector< vector<int> > edgesOnEdge;
vector< vector<double> > weightsOnEdge;
vector< vector<double> > kiteAreasOnVertex;
vector<double> dvEdge;
vector<double> dcEdge;
vector<double> areaCell;
vector<double> areaTriangle;
vector<double> angleEdge;
vector<double> meshDensity;

// }}}

// Iterators {{{
vector<pnt>::iterator pnt_itr;
vector<int>::iterator int_itr;
vector< vector<int> >::iterator vec_int_itr;
vector< vector<double> >::iterator vec_dbl_itr;
vector<double>::iterator dbl_itr;
// }}}
struct int_hasher {/*{{{*/
	size_t operator()(const int i) const {
		return (size_t)i;
	}
};/*}}}*/

/* Building/Ordering functions {{{ */
int readGridInput(const string inputFilename);
int buildUnorderedCellConnectivity();
int firstOrderingVerticesOnCell();
int buildEdges();
int orderVertexArrays();
int orderCellArrays();
int buildAreas();
int buildEdgesOnEdgeArrays();
int buildAngleEdge();
/*}}}*/

/* Output functions {{{*/
int outputGridDimensions(const string outputFilename);
int outputGridAttributes(const string outputFilename, const string inputFilename);
int outputGridCoordinates(const string outputFilename);
int outputCellConnectivity(const string outputFilename);
int outputEdgeConnectivity(const string outputFilename);
int outputVertexConnectivity(const string outputFilename);
int outputCellParameters(const string outputFilename);
int outputVertexParameters(const string outputFilename);
int outputEdgeParameters(const string outputFilename);
int outputMeshDensity(const string outputFilename);
/*}}}*/

string gen_random(const int len);

int main ( int argc, char *argv[] ) {
	int error;
	string out_name = "output.nc";
	string in_name = "grid.nc";

	cout << endl << endl;
	cout << "************************************************************" << endl;
	cout << "MPAS_MESH_CONVERTER:\n";
	cout << "  C++ version\n";
	cout << "  Convert a NetCDF file describing Cell Locations, \n";
	cout << "  Vertex Location, and Connectivity into a valid MPAS mesh.\n";
	cout << endl << endl;
	cout << "  Compiled on " << __DATE__ << " at " << __TIME__ << ".\n";
	cout << "************************************************************" << endl;
	cout << endl << endl;
	//
	//  If the input file was not specified, get it now.
	//
	if ( argc <= 1 )
	{
		cout << "\n";
		cout << "MPAS_MESH_CONVERTER:\n";
		cout << "  Please enter the NetCDF input filename.\n";

		cin >> in_name;

		cout << "\n";
		cout << "MPAS_MESH_CONVERTER:\n";
		cout << "  Please enter the output NetCDF MPAS Mesh filename.\n";

		cin >> out_name;
	}
	else if (argc == 2)
	{
		in_name = argv[1];

		cout << "\n";
		cout << "MPAS_MESH_CONVERTER:\n";
		cout << "  Output name not specified. Using default of output.nc\n";
	}
	else if (argc == 3)
	{
		in_name = argv[1];
		out_name = argv[2];
	}

	if(in_name == out_name){
		cout << "   ERROR: Input and output names are the same." << endl;
		return 1;
	}


	srand(time(NULL));

	cout << "Reading input grid." << endl;
	error = readGridInput(in_name);
	if(error) return 1;

	cout << "Build prelimiary cell connectivity." << endl;
	error = buildUnorderedCellConnectivity();
	if(error) return 1;

	cout << "Order vertices on cell." << endl;
	error = firstOrderingVerticesOnCell();
	if(error) return 1;

	cout << "Build and order edges," << endl;
	cout << "   dvEdge, and dcEdge." << endl;
	error = buildEdges();
	if(error) return 1;

	cout << "Build and order vertex arrays," << endl;
	error = orderVertexArrays();
	if(error) return 1;

	cout << "Build and order cell arrays," << endl;
	error = orderCellArrays();
	if(error) return 1;

	cout << "Build areaCell, areaTriangle, and kiteAreasOnVertex." << endl;
	error = buildAreas();
	if(error) return 1;

	cout << "Build edgesOnEdge and weightsOnEdge." << endl;
	error = buildEdgesOnEdgeArrays();
	if(error) return 1;

	cout << "Build angleEdge." << endl;
	error = buildAngleEdge();
	if(error) return 1;

	cout << "Writing grid dimensions" << endl;
	if(error = outputGridDimensions(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing grid attributes" << endl;
	if(error = outputGridAttributes(out_name, in_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing grid coordinates" << endl;
	if(error = outputGridCoordinates(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing cell connectivity" << endl;
	if(error = outputCellConnectivity(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing edge connectivity" << endl;
	if(error = outputEdgeConnectivity(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing vertex connectivity" << endl;
	if(error = outputVertexConnectivity(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing cell parameters" << endl;
	if(error = outputCellParameters(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing edge parameters" << endl;
	if(error = outputEdgeParameters(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	cout << "Writing vertex parameters" << endl;
	if(error = outputVertexParameters(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
	
	cout << "Reading and writing meshDensity" << endl;
	if(error = outputMeshDensity(out_name)){
		cout << "Error - " << error << endl;
		exit(error);
	}
}

/* Building/Ordering functions {{{ */
int readGridInput(const string inputFilename){/*{{{*/
	int nCells, nVertices, vertexDegree;
	double *xcell, *ycell,*zcell;
	double *xvertex, *yvertex,*zvertex;
	int *cellsonvertex_list;
	pnt new_location;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: readGridInput" << endl << endl;
#endif

	// Initialize range mins with huge values.
	xCellRange[0] = 1E10;
	yCellRange[0] = 1E10;
	zCellRange[0] = 1E10;
	xVertexRange[0] = 1E10;
	yVertexRange[0] = 1E10;
	zVertexRange[0] = 1E10;

	// Initialize range maxes with small values.
	xCellRange[1] = -1E10;
	yCellRange[1] = -1E10;
	zCellRange[1] = -1E10;
	xVertexRange[1] = -1E10;
	yVertexRange[1] = -1E10;
	zVertexRange[1] = -1E10;


	nCells = netcdf_mpas_read_dim(inputFilename, "nCells");
	nVertices = netcdf_mpas_read_dim(inputFilename, "nVertices");
	vertexDegree = netcdf_mpas_read_dim(inputFilename, "vertexDegree");
	vertex_degree = vertexDegree;
#ifdef _DEBUG
	cout << "   Reading on_a_sphere" << endl;
#endif
	spherical = netcdf_mpas_read_onsphere(inputFilename);
#ifdef _DEBUG
	cout << "   Reading history" << endl;
#endif
	in_history = netcdf_mpas_read_history(inputFilename);
#ifdef _DEBUG
	cout << "   Reading mesh_id" << endl;
#endif
	in_mesh_id = netcdf_mpas_read_meshid(inputFilename);

#ifdef _DEBUG
	cout << "   Reading parent_id" << endl;
#endif
	in_parent_id = netcdf_mpas_read_parentid(inputFilename);

	cout << "Read dimensions:" << endl;
	cout << "    nCells = " << nCells << endl;
	cout << "    nVertices = " << nVertices << endl;
	cout << "    vertexDegree = " << vertexDegree << endl;
	cout << "    Spherical? = " << spherical << endl;

	// Build cell center location information
	xcell = new double[nCells];
	ycell = new double[nCells];
	zcell = new double[nCells];

	netcdf_mpas_read_xyzcell ( inputFilename, nCells, xcell, ycell, zcell );

	cells.clear();
	for(int i = 0; i < nCells; i++){
		xCellRange[0] = min(xCellRange[0], xcell[i]);
		xCellRange[1] = max(xCellRange[1], xcell[i]);

		yCellRange[0] = min(yCellRange[0], ycell[i]);
		yCellRange[1] = max(yCellRange[1], ycell[i]);

		zCellRange[0] = min(zCellRange[0], zcell[i]);
		zCellRange[1] = max(zCellRange[1], zcell[i]);

		new_location = pnt(xcell[i], ycell[i], zcell[i], i);

		if(spherical) new_location.normalize();
		cells.push_back(new_location);
	}

	cout << "Built " << cells.size() << " cells." << endl;
	delete[] xcell;
	delete[] ycell;
	delete[] zcell;

	// Build vertex location information
	xvertex = new double[nVertices];
	yvertex = new double[nVertices];
	zvertex = new double[nVertices];

	netcdf_mpas_read_xyzvertex ( inputFilename, nVertices, xvertex, yvertex, zvertex );

	vertices.clear();
	for(int i = 0; i < nVertices; i++){
		xVertexRange[0] = min(xVertexRange[0], xvertex[i]);
		xVertexRange[1] = max(xVertexRange[1], xvertex[i]);

		yVertexRange[0] = min(yVertexRange[0], yvertex[i]);
		yVertexRange[1] = max(yVertexRange[1], yvertex[i]);

		zVertexRange[0] = min(zVertexRange[0], zvertex[i]);
		zVertexRange[1] = max(zVertexRange[1], zvertex[i]);

		new_location = pnt(xvertex[i], yvertex[i], zvertex[i], i);
		if(spherical) new_location.normalize();
		vertices.push_back(new_location);
	}

	cout << "Built " << vertices.size() << " vertices." << endl;
	delete[] xvertex;
	delete[] yvertex;
	delete[] zvertex;

	// Build unordered cellsOnVertex information
	cellsonvertex_list = new int[nVertices * vertexDegree];

	netcdf_mpas_read_cellsonvertex ( inputFilename, nVertices, vertexDegree, cellsonvertex_list );
	cellsOnVertex.resize(nVertices);

	for(int i = 0; i < nVertices; i++){
		for(int j = 0; j < vertexDegree; j++){
			// Subtract 1 to convert into base 0 (c index space).
			cellsOnVertex.at(i).push_back(cellsonvertex_list[i*vertexDegree + j] - 1);
		}
	}

	delete[] cellsonvertex_list;

	meshDensity.clear();
	meshDensity.resize(cells.size());
	netcdf_mpas_read_mesh_density ( inputFilename, cells.size(), &meshDensity[0]);

	xCellDistance = fabs(xCellRange[1] - xCellRange[0]);
	yCellDistance = fabs(yCellRange[1] - yCellRange[0]);
	zCellDistance = fabs(zCellRange[1] - zCellRange[0]);

	xVertexDistance = fabs(xVertexRange[1] - xVertexRange[0]);
	yVertexDistance = fabs(yVertexRange[1] - yVertexRange[0]);
	zVertexDistance = fabs(zVertexRange[1] - zVertexRange[0]);

	if(vertexDegree == 4){
		// Quads are not staggered in the y direction, so need to offset by
		// max distance + min distance
		xPeriodicFix = xCellRange[0] + xCellRange[1];
		yPeriodicFix = yCellRange[0] + yCellRange[1];
	} else {
		// Triangles can be staggered, so only offset my max distance
		xPeriodicFix = xCellRange[1];
		yPeriodicFix = yCellRange[1];
	}

#ifdef _DEBUG
	cout << "cell Mins: " << xCellRange[0] << " " << yCellRange[0] << " " << zCellRange[0] << endl;
	cout << "vertex Mins: " << xVertexRange[0] << " " << yVertexRange[0] << " " << zVertexRange[0] << endl;
	cout << "cell Maxes: " << xCellRange[1] << " " << yCellRange[1] << " " << zCellRange[1] << endl;
	cout << "vertex Maxes: " << xVertexRange[1] << " " << yVertexRange[1] << " " << zVertexRange[1] << endl;
	cout << "cell Distances: " << xCellDistance << " " << yCellDistance << " " << zCellDistance << endl;
	cout << "vertex Distances: " << xVertexDistance << " " << yVertexDistance << " " << zVertexDistance << endl;
#endif

	if(!spherical && (zCellDistance > 0.0 || zVertexDistance > 0.0)){
		cout << "ERROR:" << endl;
		cout << "  This point set is defined in the plane, but has non-zero Z coordinates." << endl;
		cout << endl;
		return 1;
	}

	return 0;
}/*}}}*/
int buildUnorderedCellConnectivity(){/*{{{*/
	// buildVerticesOnCell should assume that cellsOnVertex hasn't been ordered properly yet.
	//    It should compute the inverse of cellsOnVertex (verticesOnCell) and order the vertices CCW around the cell.
	//    This ordering should happen regardless of it the mesh is planar or spherical.
	
	int iVertex, iCell, j;
	bool add;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: buildUnorderedCellConnectivity" << endl << endl;
#endif
	
	verticesOnCell.clear();
	verticesOnCell.resize(cells.size());

	for(iVertex = 0; iVertex < vertices.size(); iVertex++){
		for(j = 0; j < cellsOnVertex.at(iVertex).size(); j++){
			iCell = cellsOnVertex.at(iVertex).at(j);
			if(iCell != -1){
				add = true;
				for(int k = 0; k < verticesOnCell.at(iCell).size(); k++){
					if(verticesOnCell.at(iCell).at(k) == iVertex){
						add = false;
					}
				}

				if(add) {
					verticesOnCell.at(iCell).push_back(iVertex);
				}
			}
		}
	}

	return 0;
}/*}}}*/
int firstOrderingVerticesOnCell(){/*{{{*/
	/*
	 * firstOrderingVerticesOnCell should order the vertices around a cell such that they are connected.
	 *		i.e. verticesOnCell.at(iCell).at(i) should be the tail of a vector pointing to
	 *		     verticesOnCell.at(iCell).at(i+1)
	 *
	 *		This is done by computing angles between vectors pointing from the cell center
	 *		to each individual vertex. The next vertex in the list, has the smallest positive angle.
	 *
	 */

	pnt vec1, vec2, cross, normal;
	pnt vertex1, vertex2;
	int iCell, iVertex1, iVertex2, j, k, l, swp_idx, swp;
	double dot, mag1, mag2, angle, min_angle;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: firstOrderingVerticesOnCell" << endl << endl;
#endif

	if(!spherical){
		normal = pnt(0.0, 0.0, 1.0);
	}

	for(iCell = 0; iCell < cells.size(); iCell++){
#ifdef _DEBUG
		cout << "new cell:" << endl;
#endif
		if(spherical){
			normal = cells.at(iCell);
		}

#ifdef _DEBUG
		cout << "  Unsorted verticesOnCell: ";
		for(j = 0; j < verticesOnCell.at(iCell).size(); j++){
			cout << verticesOnCell.at(iCell).at(j) << " ";
		}
		cout << endl;
		for(j = 0; j < verticesOnCell.at(iCell).size(); j++){
			cout << "  cellsOnVertex " << verticesOnCell.at(iCell).at(j) << ": ";
			for(k = 0; k < cellsOnVertex.at(verticesOnCell.at(iCell).at(j)).size(); k++){
				cout <<  cellsOnVertex.at(verticesOnCell.at(iCell).at(j)).at(k) << " ";
			}
			cout << endl;
		}
#endif

		// Loop over all vertices except the last two as a starting vertex
		for(j = 0; j < verticesOnCell.at(iCell).size()-1; j++){
			iVertex1 = verticesOnCell.at(iCell).at(j);

			vertex1 = vertices.at(iVertex1);

			//Check for, and fix up periodicity
			if(!spherical){
				vertex1.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
			}
			vec1 = vertex1 - cells.at(iCell);

			mag1 = vec1.magnitude();
			min_angle = 2.0*M_PI;
			angle = 0.0;
			swp_idx = -1;

			// Don't sort any vertices that have already been sorted.
			for(k = j+1; k < verticesOnCell.at(iCell).size(); k++){
#ifdef _DEBUG
				cout << "    Comparing " << j << " " << k << endl;
#endif
				iVertex2 = verticesOnCell.at(iCell).at(k);

				vertex2 = vertices.at(iVertex2);

				//Check for, and fix up periodicity
				if(!spherical){
					vertex2.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
				}
				vec2 = vertex2 - cells.at(iCell);

				mag2 = vec2.magnitude();

				cross = vec1.cross(vec2);
				dot = cross.dot(normal) / (cross.magnitude() * normal.magnitude());
#ifdef _DEBUG
				cout << "    dot = " << dot << endl;
#endif

				// Only look at vertices that are CCW from the current vertex.
				if(dot > 0){
					angle = acos(vec1.dot(vec2) / (mag1 * mag2));
#ifdef _DEBUG
					cout << "    angle = " << angle << endl;
#endif
					if(angle < min_angle){
						min_angle = angle;
						swp_idx = k;
					}
				}
			}

			if(swp_idx != -1 && swp_idx != j+1){
				swp = verticesOnCell.at(iCell).at(j+1);
				verticesOnCell.at(iCell).at(j+1) = verticesOnCell.at(iCell).at(swp_idx);
				verticesOnCell.at(iCell).at(swp_idx) = swp;
			}

#ifdef _DEBUG
			if(swp_idx != -1 && swp_idx != j+1){
				cout << "      swapped " << j << " " << swp_idx << endl;
			} else {
				cout << "      no swap" << endl;
			}
			cout << "      cellsOnVertex(" << iVertex1 << "): ";
			for(k = 0; k < cellsOnVertex.at(iVertex1).size(); k++){
				cout << cellsOnVertex.at(iVertex1).at(k) << " ";
			}
			cout << endl;
			iVertex2 = verticesOnCell.at(iCell).at(j+1);
			cout << "      cellsOnVertex(" << iVertex2 << "): ";
			for(k = 0; k < cellsOnVertex.at(iVertex2).size(); k++){
				cout << cellsOnVertex.at(iVertex2).at(k) << " ";
			}
			cout << endl;
#endif
		}
	}
	
	return 0;
}/*}}}*/
int buildEdges(){/*{{{*/
	/*
	 * buildEdges is intended to build a hash table that contains the
	 *    cellsOnEdge and verticesonEdge pairs for each edge.  The actual edge
	 *    location is not constructed at this point in time, as we need to
	 *    determine the four constituent locations for each edge before we can
	 *    compute it.
	 *
	 *    This function assumes the cellsOnVertex array is ordered such that
	 *    two consecutive cell indices make up an edge.  This isn't an issue
	 *    for triangular dual grids, but quadrilateral dual grids have an issue
	 *    where every cell is not connected with every other cell. So, the tool
	 *    that generates input data for this program needs to ensure that
	 *    ordering.
	 *
	 *    After the hash table is created, it is iterated over to create each
	 *    individual edge and ensure ordering is properly right handed.
	 *
	 */
	unordered_set<edge, edge::edge_hasher> edge_idx_hash;
	unordered_set<edge, edge::edge_hasher>::iterator edge_idx_itr;
	unordered_set<edge, edge::edge_hasher>::const_iterator edge_idx_search;

	int iCell, iVertex, iEdge, j, k, l;
	int cell1, cell2;
	int vertex1, vertex2, swp;
	int land;
	pair<unordered_set<edge,edge::edge_hasher>::iterator,bool> out_pair;
	edge new_edge;
	pnt edge_loc, normal;
	pnt cell_loc1, cell_loc2, vert_loc1, vert_loc2, dist_vec;
	pnt u_vec, v_vec, cross;
	double vert1_x_movement, vert1_y_movement;
	double vert2_x_movement, vert2_y_movement;
	double temp, dot;
	bool vert1_periodic, vert2_periodic;
	bool fixed_edge;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: buildEdges" << endl << endl;
#endif

	edge_idx_hash.clear();

	land = 0;

	for(iCell = 0; iCell < cells.size(); iCell++){
		vertex1 = verticesOnCell.at(iCell).at(verticesOnCell.at(iCell).size()-1);
		vertex2 = verticesOnCell.at(iCell).at(0);
		cell1 = iCell;
		cell2 = -1;

		// Find matching cells for vertex pair.
		for(k = 0; k < cellsOnVertex.at(vertex1).size(); k++){
			if(cellsOnVertex.at(vertex1).at(k) != iCell){
				for(l = 0; l < cellsOnVertex.at(vertex2).size(); l++){
					if(cellsOnVertex.at(vertex1).at(k) >= 0){
						if(cellsOnVertex.at(vertex1).at(k) == cellsOnVertex.at(vertex2).at(l)){
							if(cell2 == -1){
								cell2 = cellsOnVertex.at(vertex1).at(k);
							}
						}
					}
				}
			}
		}
		
		new_edge.vertex1 = min(vertex1, vertex2);
		new_edge.vertex2 = max(vertex1, vertex2);
		if(cell2 == -1){
			new_edge.cell1 = cell1;
			new_edge.cell2 = -1;
		} else {
			new_edge.cell1 = min(cell1, cell2);
			new_edge.cell2 = max(cell1, cell2);
		}

		out_pair = edge_idx_hash.insert(new_edge);

		for(j = 1; j < verticesOnCell.at(iCell).size(); j++){
			vertex1 = verticesOnCell.at(iCell).at(j-1);
			vertex2 = verticesOnCell.at(iCell).at(j);
			cell1 = iCell;
			cell2 = -1;

			// Find matching cells for vertex pair.
			for(k = 0; k < cellsOnVertex.at(vertex1).size(); k++){
				if(cellsOnVertex.at(vertex1).at(k) != iCell){
					for(l = 0; l < cellsOnVertex.at(vertex2).size(); l++){
						if(cellsOnVertex.at(vertex1).at(k) >= 0){
							if(cellsOnVertex.at(vertex1).at(k) == cellsOnVertex.at(vertex2).at(l)){
								if(cell2 == -1){
									cell2 = cellsOnVertex.at(vertex1).at(k);
								}
							}
						}
					}
				}
			}

			new_edge.vertex1 = min(vertex1, vertex2);
			new_edge.vertex2 = max(vertex1, vertex2);
			if(cell2 == -1){
				new_edge.cell1 = cell1;
				new_edge.cell2 = -1;
			} else {
				new_edge.cell1 = min(cell1, cell2);
				new_edge.cell2 = max(cell1, cell2);
			}

			out_pair = edge_idx_hash.insert(new_edge);
		}
	}

	cout << "Built " << edge_idx_hash.size() << " edge indices..." << endl;

	cellsOnEdge.clear();
	verticesOnEdge.clear();
	dvEdge.clear();
	dcEdge.clear();
	cellsOnEdge.resize(edge_idx_hash.size());
	verticesOnEdge.resize(edge_idx_hash.size());
	dvEdge.resize(edge_idx_hash.size());
	dcEdge.resize(edge_idx_hash.size());

	if(!spherical){
		normal = pnt(0.0, 0.0, 1.0);
	}

	iEdge = 0;
	for(edge_idx_itr = edge_idx_hash.begin(); edge_idx_itr != edge_idx_hash.end(); ++edge_idx_itr){
#ifdef _DEBUG
		cout << "new edge: " << endl;
#endif
		fixed_edge = false;
		cell1 = (*edge_idx_itr).cell1;
		cell2 = (*edge_idx_itr).cell2;
		vertex1 = (*edge_idx_itr).vertex1;
		vertex2 = (*edge_idx_itr).vertex2;

		cell_loc1 = cells.at(cell1);
		vert_loc1 = vertices.at(vertex1);
		vert_loc2 = vertices.at(vertex2);
		if(spherical){
			normal = cells.at(cell1);
		} else {
			// Clean up periodic edges. See mesh specification document for how periodicity is defined.
			// These edges are special in the sense that they must be across a "uniform" edge.
			// So, they are exactly the mid point between vertices.
			// Since the edge will be "owned" by whatever processor owns cell1, make sure edge is close to cell1.
			// So, fix periodicity relative to cell1.

#ifdef _DEBUG
			cout << "   Fixing periodicity of vertex 1" << endl;
#endif
			vert_loc1.fixPeriodicity(cell_loc1, xPeriodicFix, yPeriodicFix);

#ifdef _DEBUG
			cout << "   Fixing periodicity of vertex 2" << endl;
#endif
			vert_loc2.fixPeriodicity(cell_loc1, xPeriodicFix, yPeriodicFix);
		}

		if(cell2 != -1){
			cell_loc2 = cells.at(cell2);

			if(!spherical) {
#ifdef _DEBUG
				cout << "   Fixing periodicity of cell 2" << endl;
#endif
				cell_loc2.fixPeriodicity(cell_loc1, xPeriodicFix, yPeriodicFix);
				edge_loc = planarIntersect(cell_loc1, cell_loc2, vert_loc1, vert_loc2);
				dvEdge.at(iEdge) = (vert_loc2 - vert_loc1).magnitude();
				dcEdge.at(iEdge) = (cell_loc2 - cell_loc1).magnitude();
			} else { // If spherical
				edge_loc = gcIntersect(cell_loc1, cell_loc2, vert_loc1, vert_loc2);
				dvEdge.at(iEdge) = vert_loc2.sphereDistance(vert_loc1);
				dcEdge.at(iEdge) = cell_loc2.sphereDistance(cell_loc1);
			}
		} else {
			edge_loc = (vert_loc1 + vert_loc2) * 0.5;
			cell_loc2 = edge_loc;
			if(!spherical){
				// Set dv and dc Edge values on plane
				dvEdge.at(iEdge) = (vert_loc2 - vert_loc1).magnitude();
				dcEdge.at(iEdge) = sqrt(3.0) * dvEdge.at(iEdge);
			} else {
				// Set dv and dc Edge values on Sphere
				dvEdge.at(iEdge) = vert_loc2.sphereDistance(vert_loc1);
				dcEdge.at(iEdge) = sqrt(3.0) * dvEdge.at(iEdge);
			}
		}

		if(spherical) edge_loc.normalize();

		edge_loc.idx = iEdge;

#ifdef _DEBUG
		cout << "New Edge At: " << edge_loc << endl;
		cout << "         c1: " << cells.at(cell1) << endl;
		if(cell2 > -1) { 
			cout << "         c2: " << cells.at(cell2) << endl;
			cout << "    mod? c2: " << cell_loc2 << endl;
		} else {
			cout << "         c2: land" << endl;
			cout << "    mod? c2: " << cell_loc2 << endl;
		}
		cout << "         v1: " << vertices.at(vertex1) << endl;
		cout << "    mod? v1: " << vert_loc1 << endl;
		cout << "         v2: " << vertices.at(vertex2) << endl;
		cout << "    mod? v2: " << vert_loc2 << endl;
#endif

		u_vec = cell_loc2 - cell_loc1;
		v_vec = vert_loc2 - vert_loc1;

		cross = u_vec.cross(v_vec);
		dot = cross.dot(normal);

		if(dot < 0){
#ifdef _DEBUG
			cout << "   swapping vertex " << vertex1 << " and " << vertex2 << endl;
#endif
			swp = vertex2;
			vertex2 = vertex1;
			vertex1 = swp;
		}

		edges.push_back(edge_loc);
		cellsOnEdge.at(iEdge).clear();
		cellsOnEdge.at(iEdge).push_back(cell1);
		cellsOnEdge.at(iEdge).push_back(cell2);
		verticesOnEdge.at(iEdge).clear();
		verticesOnEdge.at(iEdge).push_back(vertex1);
		verticesOnEdge.at(iEdge).push_back(vertex2);

		iEdge++;
	}

	edge_idx_hash.clear();

	return 0;
}/*}}}*/
int orderVertexArrays(){/*{{{*/
	/*
	 * orderVertexArrays builds and orders the connectivity arrays for vertices.
	 *		This includes edgesOnVertex and cellsOnvertex.
	 *      First, edgeSOnVertex is built and ordered correctly, then using
	 *      that ordering cellsOnVertex is built and ordered correctly as well.
	 */
	int iEdge, iVertex, vertex1, vertex2, cell1, cell2, edge1, edge2;
	int i, j, k, l, swp_idx, swp;
	pnt normal;
	pnt cross;
	pnt vec1, vec2;
	pnt cell_loc, edge_loc1, edge_loc2;
	double mag1, mag2, min_angle, angle;
	double dvEdge, dot, area;
	bool fixed_area;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: orderVertexArrays" << endl << endl;
#endif

	edgesOnVertex.clear();
	edgesOnVertex.resize(vertices.size());
	cellsOnVertex.clear();
	cellsOnVertex.resize(vertices.size());

	// Get lists of edges for each vertex
	for(iEdge = 0; iEdge < edges.size(); iEdge++){/*{{{*/
		vertex1 = verticesOnEdge.at(iEdge).at(0);
		vertex2 = verticesOnEdge.at(iEdge).at(1);

		edgesOnVertex.at(vertex1).push_back(iEdge);
		edgesOnVertex.at(vertex2).push_back(iEdge);
	}/*}}}*/

	if(!spherical){
		normal = pnt(0.0, 0.0, 1.0);
	}

	// Order edges counter-clockwise
	for(iVertex = 0; iVertex < vertices.size(); iVertex++){/*{{{*/
		if(spherical){
			normal = vertices.at(iVertex);
		}

		//Loop over all edges except the last two as a starting edge.
		for(j = 0; j < edgesOnVertex.at(iVertex).size(); j++){
			edge1 = edgesOnVertex.at(iVertex).at(j);

			edge_loc1 = edges.at(edge1);

			// Fix edge1 periodicity
			if(!spherical){
				edge_loc1.fixPeriodicity(vertices.at(iVertex), xPeriodicFix, yPeriodicFix);
			}
			vec1 = edge_loc1 - vertices.at(iVertex);

			mag1 = vec1.magnitude();
			min_angle = 2.0*M_PI;
			angle = 0.0;
			swp_idx = -1;

			//Don't sort any edges that have already been sorted.
			for(k = j+1; k < edgesOnVertex.at(iVertex).size(); k++){
				edge2 = edgesOnVertex.at(iVertex).at(k);

				edge_loc2 = edges.at(edge2);

				// Fix edge2 periodicity
				if(!spherical){
					edge_loc2.fixPeriodicity(vertices.at(iVertex), xPeriodicFix, yPeriodicFix);
				}
				vec2 = edge_loc2 - vertices.at(iVertex);
				mag2 = vec2.magnitude();

				cross = vec1.cross(vec2);
				dot = cross.dot(normal) / (cross.magnitude() * normal.magnitude());
				angle = acos(vec1.dot(vec2) / (mag1 * mag2));

				// Only look at vertices that are CCW from the current edge.
				if(dot > 0){
					angle = acos(vec1.dot(vec2) / (mag1 * mag2));
					if(angle < min_angle){
						min_angle = angle;
						swp_idx = k;
					}
				}
			}

			if(swp_idx != -1 && swp_idx != j+1){
				swp = edgesOnVertex.at(iVertex).at(j+1);
				edgesOnVertex.at(iVertex).at(j+1) = edgesOnVertex.at(iVertex).at(swp_idx);
				edgesOnVertex.at(iVertex).at(swp_idx) = swp;
			}
		}

		cellsOnVertex.at(iVertex).clear();

		// Using the ordered edges. Buld cellsOnVertex in the correct order.
		for(j = 0; j < edgesOnVertex.at(iVertex).size(); j++){
			edge1 = edgesOnVertex.at(iVertex).at(j);
			fixed_area = false;

			// Get cell id and add it to list of cells
			if(iVertex == verticesOnEdge.at(edge1).at(0)){
				cell1 = cellsOnEdge.at(edge1).at(0);
				cellsOnVertex.at(iVertex).push_back( cellsOnEdge.at(edge1).at(0) );
			} else {
				cell1 = cellsOnEdge.at(edge1).at(1);
				cellsOnVertex.at(iVertex).push_back( cellsOnEdge.at(edge1).at(1) );
			}
		}
	}/*}}}*/

	return 0;
}/*}}}*/
int orderCellArrays(){/*{{{*/
	/*
	 * orderCellArrays assumes verticesOnCell are ordered CCW already.
	 *
	 */
	int iCell, iVertex, iEdge, iEdge2, add_cell;	
	int cell1, cell2, vertex1, vertex2, prev_vertex;
	int edge_idx;
	int i, j, k, swp_idx;
	pnt normal, cross;
	pnt vec1, vec2;
	pnt vert_loc, edge_loc, cell_loc;
	pnt next_edge_loc;
	double swp;
	double dot, mag1, mag2, angle, min_angle;
	bool found;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: orderCellArrays" << endl << endl;
#endif

	maxEdges = 0;

	verticesOnCell.clear();
	edgesOnCell.clear();
	cellsOnCell.clear();

	verticesOnCell.resize(cells.size());
	edgesOnCell.resize(cells.size());
	cellsOnCell.resize(cells.size());

	if(!spherical){
		normal = pnt(0.0, 0.0, 1.0);
	}

	// First, build full list of edges on cell.
	for(iEdge = 0; iEdge < edges.size(); iEdge++){
		cell1 = cellsOnEdge.at(iEdge).at(0);	
		cell2 = cellsOnEdge.at(iEdge).at(1);	

		edgesOnCell.at(cell1).push_back(iEdge);
		if(cell2 != -1){
			edgesOnCell.at(cell2).push_back(iEdge);
		}
	}

	// Loop over all cells.
	for(iCell = 0; iCell < cells.size(); iCell++){
#ifdef _DEBUG
		cout << "New Cell " << cells.at(iCell) << endl;
#endif
		if(spherical){
			normal = cells.at(iCell);
		}



		// Order all edges in CCW relative to the first edge.
		// Loop over all vertices except the last two as a starting vertex.
		for(j = 0; j < edgesOnCell.at(iCell).size(); j++){
			iEdge = edgesOnCell.at(iCell).at(j);
			edge_loc = edges.at(iEdge);

			// Add cell and vertex from first edge.
			// Add cell across edge to cellsOnCell
			// Also, add vertex that is CCW relative to current edge location.
			if(cellsOnEdge.at(iEdge).at(0) == iCell){
				cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(1));
				verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(1));
			} else {
				cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(0));
				verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(0));
			}


			if(!spherical){
#ifdef _DEBUG
				cout << "   checking periodicity on edge 1" << endl;
#endif
				edge_loc.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
			}

			vec1 = edge_loc - cells.at(iCell);
			mag1 = vec1.magnitude();

			min_angle = 2.0 * M_PI;
			angle = 0.0;
			swp_idx = -1;

			for(k = j+1; k < edgesOnCell.at(iCell).size(); k++){
				iEdge2 = edgesOnCell.at(iCell).at(k);

				next_edge_loc = edges.at(iEdge2);

				if(!spherical){
#ifdef _DEBUG
					cout << "   checking periodicity on edge 2" << endl;
#endif
					next_edge_loc.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
				}

				vec2 = next_edge_loc - cells.at(iCell);
				mag2 = vec2.magnitude();

				cross = vec1.cross(vec2);
				dot = cross.dot(normal) / (cross.magnitude() * normal.magnitude());

				// Only look at edges that are CCW from the current edge
				if(dot > 0){
					angle = acos(vec1.dot(vec2) / (mag1 * mag2));
					if(angle < min_angle){
						min_angle = angle;
						swp_idx = k;
					}
				}
			}

			if(swp_idx != -1 && swp_idx != j+1){
				swp = edgesOnCell.at(iCell).at(j+1);
				edgesOnCell.at(iCell).at(j+1) = edgesOnCell.at(iCell).at(swp_idx);
				edgesOnCell.at(iCell).at(swp_idx) = swp;

				/* 
				iEdge = edgesOnCell.at(iCell).at(j+1);
				// Add cell across edge to cellsOnCell
				// Also, add vertex that is CCW relative to current edge location.
				if(cellsOnEdge.at(iEdge).at(0) == iCell){
					cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(1));
					verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(1));
				} else {
					cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(0));
					verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(0));
				} // */
			} else {
				/*
				iEdge = edgesOnCell.at(iCell).at(j+1);
				// Add cell across edge to cellsOnCell
				// Also, add vertex that is CCW relative to current edge location.
				if(cellsOnEdge.at(iEdge).at(0) == iCell){
					cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(1));
					verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(1));
				} else {
					cellsOnCell.at(iCell).push_back(cellsOnEdge.at(iEdge).at(0));
					verticesOnCell.at(iCell).push_back(verticesOnEdge.at(iEdge).at(0));
				} // */
			}
		}


#ifdef _DEBUG
		cout << "   cellsOnCell: ";
		for(i = 0; i < cellsOnCell.at(iCell).size(); i++){
			cout << cellsOnCell.at(iCell).at(i) << " ";
		}
		cout << endl;
		cout << "   verticesOnCell: ";
		for(i = 0; i < verticesOnCell.at(iCell).size(); i++){
			cout << verticesOnCell.at(iCell).at(i) << " ";
		}
		cout << endl;
		cout << "   edgesOnCell: ";
		for(i = 0; i < edgesOnCell.at(iCell).size(); i++){
			cout << edgesOnCell.at(iCell).at(i) << " ";
		}
		cout << endl;
#endif

		maxEdges = max(maxEdges, (int)edgesOnCell.at(iCell).size());
	}

	return 0;
}/*}}}*/
int buildAreas(){/*{{{*/
	/*
	 * buildAreas constructs the area arrays.
	 *    This includes areaCell, areaTriangle, and kiteAreasOnVertex
	 *
	 * Before constructing areaCell, the cell is checked for completeness.
	 *    This is accomplished by looping over each edge. For each edge,
	 *    the angle between it's vertices is computed and added to a running total.
	 *    If the total is significantly less than 2*Pi, the cell is not complete.
	 *
	 * Non-complete cells are given a negative area, to ease removal at a later stage.
	 */
	int iVertex, iCell, iEdge, i, j;
	int vertex1, vertex2;
	int edge1, edge2;
	pnt vert_loc1, vert_loc2;
	pnt edge_loc1, edge_loc2;
	pnt cell_loc;
	pnt vec1, vec2;
	double angle_sum;
	double angle;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: buildAreas" << endl << endl;
#endif

	areaCell.clear();
	areaTriangle.clear();
	kiteAreasOnVertex.clear();

	areaCell.resize(cells.size());
	areaTriangle.resize(vertices.size());
	kiteAreasOnVertex.resize(vertices.size());

	for(iCell = 0; iCell < cells.size(); iCell++){
		areaCell.at(iCell) = 0.0;

		// Check for full cells, by summing the angles between the two vertices of each edge.
		// If they sum to something close to 2*Pi, it's a full cell, otherwise it's not.
		// Non-complete cells get negative area so we can easily remove later.
		angle_sum = 0.0;
		for(j = 0; j < edgesOnCell.at(iCell).size()-1; j++){
			iEdge = edgesOnCell.at(iCell).at(j);
			vertex1 = verticesOnEdge.at(iEdge).at(0);
			vertex2 = verticesOnEdge.at(iEdge).at(1);

			vert_loc1 = vertices.at(vertex1);
			vert_loc2 = vertices.at(vertex2);

			if(!spherical){
				vert_loc1.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
				vert_loc2.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
			}

			vec1 = vert_loc1 - cells.at(iCell);
			vec2 = vert_loc2 - cells.at(iCell);

			angle = acos( vec2.dot(vec1) / (vec1.magnitude() * vec2.magnitude()));
			angle_sum += angle;
		}

		if(angle_sum > M_2_PI * 0.8){ // Not likely to be a full cell if angle_sum is less than 80% of 2*Pi
			for(j = 0; j < edgesOnCell.at(iCell).size(); j++){
				iEdge = edgesOnCell.at(iCell).at(j);

				if(cellsOnEdge.at(iEdge).at(0) == iCell){
					vertex1 = verticesOnEdge.at(iEdge).at(0);
					vertex2 = verticesOnEdge.at(iEdge).at(1);
				} else {
					vertex1 = verticesOnEdge.at(iEdge).at(1);
					vertex2 = verticesOnEdge.at(iEdge).at(0);
				}

				vert_loc1 = vertices.at(vertex1);
				vert_loc2 = vertices.at(vertex2);

				if(!spherical){
					vert_loc1.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
					vert_loc2.fixPeriodicity(cells.at(iCell), xPeriodicFix, yPeriodicFix);
					areaCell.at(iCell) += planarTriangleArea(cells.at(iCell), vert_loc1, vert_loc2);
				} else {
					areaCell.at(iCell) += sphericalTriangleArea(cells.at(iCell), vert_loc1, vert_loc2);
				}
			}
		} else {
			cout << "   Non complete cell found at " << iCell << endl;
			cout << "   angles (rad) sum to " << angle_sum << endl;
			cout << "   angles (deg) sum to " << angle_sum*180.0/M_PI << endl;
			areaCell.at(iCell) = -1;
		}
	}

	for(iVertex = 0; iVertex < vertices.size(); iVertex++){
		areaTriangle.at(iVertex) = 0.0;
		kiteAreasOnVertex.at(iVertex).resize(cellsOnVertex.at(iVertex).size());
		for(j = 0; j < cellsOnVertex.at(iVertex).size(); j++){
			kiteAreasOnVertex.at(iVertex).at(j) = 0.0;
			iCell = cellsOnVertex.at(iVertex).at(j);

			if(iCell != -1){
				edge1 = edgesOnVertex.at(iVertex).at(j);

				if(j == cellsOnVertex.at(iVertex).size()-1){
					edge2 = edgesOnVertex.at(iVertex).at(0);
				} else {
					edge2 = edgesOnVertex.at(iVertex).at(j+1);
				}

				cell_loc = cells.at(iCell);
				edge_loc1 = edges.at(edge1);
				edge_loc2 = edges.at(edge2);

				if(!spherical){
					cell_loc.fixPeriodicity(vertices.at(iVertex), xPeriodicFix, yPeriodicFix);
					edge_loc1.fixPeriodicity(vertices.at(iVertex), xPeriodicFix, yPeriodicFix);
					edge_loc2.fixPeriodicity(vertices.at(iVertex), xPeriodicFix, yPeriodicFix);
					kiteAreasOnVertex.at(iVertex).at(j) += planarTriangleArea(vertices.at(iVertex), edge_loc1, cell_loc);
					kiteAreasOnVertex.at(iVertex).at(j) += planarTriangleArea(vertices.at(iVertex), cell_loc, edge_loc2);
				} else {
					kiteAreasOnVertex.at(iVertex).at(j) += sphericalTriangleArea(vertices.at(iVertex), edge_loc1, cell_loc);
					kiteAreasOnVertex.at(iVertex).at(j) += sphericalTriangleArea(vertices.at(iVertex), cell_loc, edge_loc2);
				}

				areaTriangle.at(iVertex) += kiteAreasOnVertex.at(iVertex).at(j);
			}
		}
	}

	return 0;
}/*}}}*/
int buildEdgesOnEdgeArrays(){/*{{{*/
	/*
	 * buildEdgesOnEdgeArrays builds both edgesOnEdge and weightsOnEdge
	 *
	 * The weights correspond to edgesOnEdge and allow MPAS to reconstruct the
	 * edge perpendicular (previously defined as v) velocity using the edge
	 * neighbors
	 *
	 * Weight formulation is defined in J. Thurburn, et al. JCP 2009
	 * Numerical representation of geostrophic modes on arbitrarily
	 * structured C-grids
	 *
	 * Before using this function, dcEdge, dvEdge, areaCell, and kiteAreasOnVertex
	 * all need to be computed correctly.
	 */
	int iEdge, iCell, cell1, cell2;
	int i, j, k;
	int shared_vertex;
	int last_edge, cur_edge;
	double area_sum;
	bool found;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: buildEdgesOnEdgeArrays" << endl << endl;
#endif

	edgesOnEdge.clear();
	edgesOnEdge.resize(edges.size());

	weightsOnEdge.clear();
	weightsOnEdge.resize(edges.size());

	for(iEdge = 0; iEdge < edges.size(); iEdge++){
#ifdef _DEBUG
		cout << "New edge: " << edges.at(iEdge) << endl;
#endif
		cell1 = cellsOnEdge.at(iEdge).at(0);
		cell2 = cellsOnEdge.at(iEdge).at(1);
		found = false;

		// Loop over cell 1. Starting from the edge after the current edge, add
		// all edges counter clockwise around the cell.  Don't add the current
		// edge to the list.
#ifdef _DEBUG
		cout << "   On cell1: " << cell1 << endl;
#endif
		last_edge = iEdge;
		area_sum = 0;
		for(i = 0; i < edgesOnCell.at(cell1).size(); i++){
#ifdef _DEBUG
			cout << "      checking edge: " << edgesOnCell.at(cell1).at(i) << endl;
#endif
			if(edgesOnCell.at(cell1).at(i) == iEdge){
				found = true;
#ifdef _DEBUG
				cout << "        -- found -- " << endl;
#endif
			}

			if(found && edgesOnCell.at(cell1).at(i) != iEdge){
				cur_edge = edgesOnCell.at(cell1).at(i);
				edgesOnEdge.at(iEdge).push_back(cur_edge);

				// Find shared vertex between newly added edge
				// and last_edge
				if(verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(0) ||
						verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(1)){

					shared_vertex = verticesOnEdge.at(last_edge).at(0);

				} else if(verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(0) ||
						verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(1)){

					shared_vertex = verticesOnEdge.at(last_edge).at(1);
				}

				// Find cell 1 on shared vertex (to get kite area)
				for(j = 0; j < cellsOnVertex.at(shared_vertex).size(); j++){
					iCell = cellsOnVertex.at(shared_vertex).at(j);

					if(iCell == cell1){
						area_sum += kiteAreasOnVertex.at(shared_vertex).at(j) / areaCell.at(cell1);
					}
				}

				if(cell1 == cellsOnEdge.at(cur_edge).at(0)){
					weightsOnEdge.at(iEdge).push_back( 1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
				} else {
					weightsOnEdge.at(iEdge).push_back( -1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
				}

				last_edge = edgesOnCell.at(cell1).at(i);
#ifdef _DEBUG
				cout << "        added " << edgesOnCell.at(cell1).at(i) << endl;
#endif
			}
		}
		if(!found) {
			cout << "Error finding edge " << iEdge << " on cell " << cell1 << " 1" << endl;
			return 1;
		}

		for(i = 0; i < edgesOnCell.at(cell1).size() && found; i++){
#ifdef _DEBUG
			cout << "      checking edge: " << edgesOnCell.at(cell1).at(i) << endl;
#endif
			if(edgesOnCell.at(cell1).at(i) == iEdge){
#ifdef _DEBUG
				cout << "        -- found -- " << endl;
#endif
				found = false;
			}

			if(found && edgesOnCell.at(cell1).at(i) != iEdge){
				cur_edge = edgesOnCell.at(cell1).at(i);
				edgesOnEdge.at(iEdge).push_back(cur_edge);

				// Find shared vertex between newly added edge
				// and last_edge
				if(verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(0) ||
						verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(1)){

					shared_vertex = verticesOnEdge.at(last_edge).at(0);

				} else if(verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(0) ||
						verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(1)){

					shared_vertex = verticesOnEdge.at(last_edge).at(1);
				}

				// Find cell 1 on shared vertex (to get kite area)
				for(j = 0; j < cellsOnVertex.at(shared_vertex).size(); j++){
					iCell = cellsOnVertex.at(shared_vertex).at(j);

					if(iCell == cell1){
						area_sum += kiteAreasOnVertex.at(shared_vertex).at(j) / areaCell.at(cell1);
					}
				}

				if(cell1 == cellsOnEdge.at(cur_edge).at(0)){
					weightsOnEdge.at(iEdge).push_back( 1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
				} else {
					weightsOnEdge.at(iEdge).push_back( -1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
				}

				last_edge = edgesOnCell.at(cell1).at(i);
#ifdef _DEBUG
				cout << "        added " << edgesOnCell.at(cell1).at(i) << endl;
#endif
			}
		}
		if(found) {
			cout << "Error finding edge " << iEdge << " on cell " << cell1 << " 1" << endl;
			return 1;
		}

		// Check if cell1 is a real cell or not.
		if(cell2 > -1){
			last_edge = iEdge;
			area_sum = 0.0;
#ifdef _DEBUG
			cout << "   On cell2: " << cell2 << endl;
#endif
			found = false;
			// Loop over cell 2. Starting from the edge after the current edge,
			// add all edges counter clockwise around the cell.  Don't add the
			// current edge to the list.
			for(i = 0; i < edgesOnCell.at(cell2).size(); i++){
#ifdef _DEBUG
				cout << "      checking edge: " << edgesOnCell.at(cell2).at(i) << endl;
#endif
				if(edgesOnCell.at(cell2).at(i) == iEdge){
					found = true;
#ifdef _DEBUG
					cout << "        -- found -- " << endl;
#endif
				}

				if(found && edgesOnCell.at(cell2).at(i) != iEdge){
					cur_edge = edgesOnCell.at(cell2).at(i);
					edgesOnEdge.at(iEdge).push_back(cur_edge);

					// Find shared vertex between newly added edge
					// and last_edge
					if(verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(0) ||
							verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(1)){

						shared_vertex = verticesOnEdge.at(last_edge).at(0);

					} else if(verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(0) ||
							verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(1)){

						shared_vertex = verticesOnEdge.at(last_edge).at(1);
					}

					// Find cell 1 on shared vertex (to get kite area)
					for(j = 0; j < cellsOnVertex.at(shared_vertex).size(); j++){
						iCell = cellsOnVertex.at(shared_vertex).at(j);

						if(iCell == cell2){
							area_sum += kiteAreasOnVertex.at(shared_vertex).at(j) / areaCell.at(cell2);
						}
					}

					if(cell1 == cellsOnEdge.at(cur_edge).at(0)){
						weightsOnEdge.at(iEdge).push_back( 1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
					} else {
						weightsOnEdge.at(iEdge).push_back( -1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
					}

					last_edge = edgesOnCell.at(cell2).at(i);
#ifdef _DEBUG
					cout << "        added " << edgesOnCell.at(cell2).at(i) << endl;
#endif
				}
			}
			if(!found) {
				cout << "Error finding edge " << iEdge << " on cell " << cell2 << " 2" << endl;
				return 1;
			}

			for(i = 0; i < edgesOnCell.at(cell2).size(); i++){
#ifdef _DEBUG
				cout << "      checking edge: " << edgesOnCell.at(cell2).at(i) << endl;
#endif
				if(edgesOnCell.at(cell2).at(i) == iEdge){
					found = false;
#ifdef _DEBUG
					cout << "        -- found -- " << endl;
#endif
				}

				if(found && edgesOnCell.at(cell2).at(i) != iEdge){
					cur_edge = edgesOnCell.at(cell2).at(i);
					edgesOnEdge.at(iEdge).push_back(cur_edge);

					// Find shared vertex between newly added edge
					// and last_edge
					if(verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(0) ||
							verticesOnEdge.at(last_edge).at(0) == verticesOnEdge.at(cur_edge).at(1)){

						shared_vertex = verticesOnEdge.at(last_edge).at(0);

					} else if(verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(0) ||
							verticesOnEdge.at(last_edge).at(1) == verticesOnEdge.at(cur_edge).at(1)){

						shared_vertex = verticesOnEdge.at(last_edge).at(1);
					}

					// Find cell 1 on shared vertex (to get kite area)
					for(j = 0; j < cellsOnVertex.at(shared_vertex).size(); j++){
						iCell = cellsOnVertex.at(shared_vertex).at(j);

						if(iCell == cell2){
							area_sum += kiteAreasOnVertex.at(shared_vertex).at(j) / areaCell.at(cell2);
						}
					}

					if(cell1 == cellsOnEdge.at(cur_edge).at(0)){
						weightsOnEdge.at(iEdge).push_back( 1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
					} else {
						weightsOnEdge.at(iEdge).push_back( -1.0 * (0.5 - area_sum) * dvEdge.at(cur_edge) / dcEdge.at(iEdge) );
					}

					last_edge = edgesOnCell.at(cell2).at(i);
#ifdef _DEBUG
					cout << "        added " << edgesOnCell.at(cell2).at(i) << endl;
#endif
				}
			}
			if(found) {
				cout << "Error finding edge " << iEdge << " on cell " << cell2 << " 2" << endl;
				return 1;
			}
		}
	}

	return 0;
}/*}}}*/
int buildAngleEdge(){/*{{{*/
	/*
	 * buildAngleEdge constructs angle edge for each edge.
	 *
	 * angleEdge is either:
	 * 1. The angle the positive tangential direction (v)
	 *    makes with the local northward direction.
	 *    or
	 * 2. The angles the positive normal direction (u)
	 * 	  makes with the local eastward direction.
	 * 	  
	 * 	  In a plane, local eastward is defined as the x axis, and
	 * 	  nortward is defined as the y axis.
	 */
	int iEdge;
	int cell1, cell2;
	int vertex1, vertex2;
	pnt np, x_axis, normal;
	pnt cell_loc1, cell_loc2;
	pnt vertex_loc1, vertex_loc2;
	double angle, sign;

#ifdef _DEBUG
	cout << endl << endl << "Begin function: buildAngleEdge" << endl << endl;
#endif

	angleEdge.clear();
	angleEdge.resize(edges.size());

	x_axis = pnt(1.0, 0.0, 0.0);

	for(iEdge = 0; iEdge < edges.size(); iEdge++){

#ifdef _DEBUG
		cout << "New edge: " << edges.at(iEdge) << endl;
#endif
		cell1 = cellsOnEdge.at(iEdge).at(0);
		cell2 = cellsOnEdge.at(iEdge).at(1);

		vertex1 = verticesOnEdge.at(iEdge).at(0);
		vertex2 = verticesOnEdge.at(iEdge).at(1);

		cell_loc1 = cells.at(cell1);
		if(cell2 != -1){
			cell_loc2 = cells.at(cell2);
		} else {
			cell_loc2 = edges.at(iEdge);
		}

		vertex_loc1 = vertices.at(vertex1);
		vertex_loc2 = vertices.at(vertex2);

		if(!spherical){
			cell_loc2.fixPeriodicity(cell_loc1, xPeriodicFix, yPeriodicFix);	

			normal = cell_loc2 - cell_loc1;
			angleEdge.at(iEdge) = acos( x_axis.dot(normal) / (x_axis.magnitude() * normal.magnitude()));
		} else {

			np = pntFromLatLon(edges.at(iEdge).getLat()+0.05, edges.at(iEdge).getLon());
			np.normalize();

#ifdef _DEBUG
			cout << "     NP: " << np << endl;
#endif

			angle = (vertex_loc2.getLat() - vertex_loc1.getLat()) / dvEdge.at(iEdge);
			angle = max( min(angle, 1.0), -1.0);
			angle = acos(angle);

#ifdef _DEBUG
			cout << "  angle: " << angle << endl;
#endif

			sign = planeAngle(edges.at(iEdge), np, vertex_loc2, edges.at(iEdge));
			if(sign != 0.0){
				sign = sign / fabs(sign);
			} else {
				sign = 1.0;
			}


#ifdef _DEBUG
			cout << "  sign : " << sign << endl;
			cout << "  a*s  : " << angle * sign << endl;
#endif

			angle = angle * sign;
			if(angle > M_PI) angle = angle - 2.0 * M_PI;
			if(angle < -M_PI) angle = angle + 2.0 * M_PI;

#ifdef _DEBUG
			cout << " fangle: " << angle << endl;
#endif

			angleEdge.at(iEdge) = angle;
		}
	}

	return 0;
}/*}}}*/
/*}}}*/

/* Output functions {{{*/
int outputGridDimensions( const string outputFilename ){/*{{{*/
	/************************************************************************
	 *
	 * This function writes the grid dimensions to the netcdf file named
	 * outputFilename
	 *
	 * **********************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Replace, NULL, 0, NcFile::Offset64Bits);

	int junk;

	nCells = cells.size();

	/*
	for(vec_int_itr = edgesOnCell.begin(); vec_int_itr != edgesOnCell.end(); ++vec_int_itr){
		maxEdges = std::max(maxEdges, (int)(*vec_int_itr).size());	
	}*/
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// define dimensions
	NcDim *nCellsDim;
	NcDim *nEdgesDim;
	NcDim *nVerticesDim;
	NcDim *maxEdgesDim;
	NcDim *maxEdges2Dim;
	NcDim *TWODim;
	NcDim *THREEDim;
	NcDim *vertexDegreeDim;
	NcDim *timeDim;
	
	// write dimensions
	if (!(nCellsDim =		grid.add_dim(	"nCells",		cells.size())		)) return NC_ERR;
	if (!(nEdgesDim =		grid.add_dim(	"nEdges",		edges.size())		)) return NC_ERR;
	if (!(nVerticesDim =	grid.add_dim(	"nVertices",	vertices.size())	)) return NC_ERR;
	if (!(maxEdgesDim =		grid.add_dim(	"maxEdges",		maxEdges)			)) return NC_ERR;
	if (!(maxEdges2Dim =	grid.add_dim(	"maxEdges2",	maxEdges*2)			)) return NC_ERR;
	if (!(TWODim =			grid.add_dim(	"TWO",			2)					)) return NC_ERR;
	if (!(vertexDegreeDim = grid.add_dim(   "vertexDegree", vertex_degree)		)) return NC_ERR;
	if (!(timeDim = 		grid.add_dim(   "Time")								)) return NC_ERR;

	grid.close();
	
	// file closed when file obj goes out of scope
	return 0;
}/*}}}*/
int outputGridAttributes( const string outputFilename, const string inputFilename ){/*{{{*/
	/************************************************************************
	 *
	 * This function writes the grid dimensions to the netcdf file named
	 * outputFilename
	 *
	 * **********************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);

	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	NcBool sphereAtt, radiusAtt;
	NcBool history, id, spec, conventions, source, parent_id;
	string history_str = "";
	string id_str = "";
	string parent_str ="";
	
	// write attributes
	if(!spherical){
		if (!(sphereAtt = grid.add_att(   "on_a_sphere", "NO\0"))) return NC_ERR;
		if (!(radiusAtt = grid.add_att(   "sphere_radius", 0.0))) return NC_ERR;
	} else {
		if (!(sphereAtt = grid.add_att(   "on_a_sphere", "YES\0"))) return NC_ERR;
		if (!(radiusAtt = grid.add_att(   "sphere_radius", 1.0))) return NC_ERR;
	}

	history_str += "MpasMeshConverter.x ";
	history_str += inputFilename;
	history_str += " ";
	history_str += outputFilename;
	if(in_history != ""){
		history_str += "\n";
		history_str += in_history;
	}

	if(in_mesh_id != "" ){
		parent_str = in_mesh_id;
		if(in_parent_id != ""){
			parent_str += "\n";
			parent_str += in_parent_id;
		}
		if (!(id = grid.add_att(   "parent_id", parent_str.c_str() ))) return NC_ERR;
	}
	id_str = gen_random(ID_LEN);

	if (!(history = grid.add_att(   "history", history_str.c_str() ))) return NC_ERR;
	if (!(spec = grid.add_att(   "mesh_spec", (double)MESH_SPEC ))) return NC_ERR;
	if (!(conventions = grid.add_att(   "Conventions", "MPAS" ))) return NC_ERR;
	if (!(source = grid.add_att(   "source", "MpasMeshConverter.x" ))) return NC_ERR;
	if (!(id = grid.add_att(   "mesh_id", id_str.c_str() ))) return NC_ERR;

	grid.close();
	
	// file closed when file obj goes out of scope
	return 0;
}/*}}}*/
int outputGridCoordinates( const string outputFilename) {/*{{{*/
	/************************************************************************
	 *
	 * This function writes the grid coordinates to the netcdf file named
	 * outputFilename
	 * This includes all cell centers, vertices, and edges.
	 * Both cartesian and lat,lon, as well as all of their indices
	 *
	 * **********************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nCellsDim = grid.get_dim( "nCells" );
	NcDim *nEdgesDim = grid.get_dim( "nEdges" );
	NcDim *nVerticesDim = grid.get_dim( "nVertices" );

	int nCells = nCellsDim->size();
	int nEdges = nEdgesDim->size();
	int nVertices = nVerticesDim->size();

	//Define nc variables
	NcVar *xCellVar, *yCellVar, *zCellVar, *xEdgeVar, *yEdgeVar, *zEdgeVar, *xVertexVar, *yVertexVar, *zVertexVar;
	NcVar *lonCellVar, *latCellVar, *lonEdgeVar, *latEdgeVar, *lonVertexVar, *latVertexVar;
	NcVar *idx2cellVar, *idx2edgeVar, *idx2vertexVar;

	int i;
	
	double *x, *y, *z, *lat, *lon;
	int *idxTo;

	// Build and write cell coordinate arrays
	x = new double[nCells];
	y = new double[nCells];
	z = new double[nCells];
	lat = new double[nCells];
	lon = new double[nCells];
	idxTo = new int[nCells];
	i = 0;
	for(pnt_itr = cells.begin(); pnt_itr != cells.end(); ++pnt_itr){
		x[i] = (*pnt_itr).x;
		y[i] = (*pnt_itr).y;
		z[i] = (*pnt_itr).z;
		if(!spherical){
			lat[i] = 0.0;
			lon[i] = 0.0;
		} else {
			lat[i] = (*pnt_itr).getLat();
			lon[i] = (*pnt_itr).getLon();
		}
		idxTo[i] = (*pnt_itr).idx+1;

		i++;
	}
	if (!(latCellVar = grid.add_var("latCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!latCellVar->put(lat,nCells)) return NC_ERR;
	if (!(lonCellVar = grid.add_var("lonCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!lonCellVar->put(lon,nCells)) return NC_ERR;
	if (!(xCellVar = grid.add_var("xCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!xCellVar->put(x,nCells)) return NC_ERR;
	if (!(yCellVar = grid.add_var("yCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!yCellVar->put(y,nCells)) return NC_ERR;
	if (!(zCellVar = grid.add_var("zCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!zCellVar->put(z,nCells)) return NC_ERR;
	if (!(idx2cellVar = grid.add_var("indexToCellID", ncInt, nCellsDim))) return NC_ERR;
	if (!idx2cellVar->put(idxTo,nCells)) return NC_ERR;
	delete[] x;
	delete[] y;
	delete[] z;
	delete[] lat;
	delete[] lon;
	delete[] idxTo;
	
	//Build and write edge coordinate arrays
	x = new double[nEdges];
	y = new double[nEdges];
	z = new double[nEdges];
	lat = new double[nEdges];
	lon = new double[nEdges];
	idxTo = new int[nEdges];

	i = 0;
	for(pnt_itr = edges.begin(); pnt_itr != edges.end(); ++pnt_itr){
		x[i] = (*pnt_itr).x;
		y[i] = (*pnt_itr).y;
		z[i] = (*pnt_itr).z;
		if(!spherical){
			lat[i] = 0.0;
			lon[i] = 0.0;
		} else {
			lat[i] = (*pnt_itr).getLat();
			lon[i] = (*pnt_itr).getLon();
		}
		idxTo[i] = (*pnt_itr).idx+1;

		i++;
	}
	if (!(latEdgeVar = grid.add_var("latEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!latEdgeVar->put(lat,nEdges)) return NC_ERR;
	if (!(lonEdgeVar = grid.add_var("lonEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!lonEdgeVar->put(lon,nEdges)) return NC_ERR;
	if (!(xEdgeVar = grid.add_var("xEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!xEdgeVar->put(x,nEdges)) return NC_ERR;
	if (!(yEdgeVar = grid.add_var("yEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!yEdgeVar->put(y,nEdges)) return NC_ERR;
	if (!(zEdgeVar = grid.add_var("zEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!zEdgeVar->put(z,nEdges)) return NC_ERR;
	if (!(idx2edgeVar = grid.add_var("indexToEdgeID", ncInt, nEdgesDim))) return NC_ERR;
	if (!idx2edgeVar->put(idxTo, nEdges)) return NC_ERR;
	delete[] x;
	delete[] y;
	delete[] z;
	delete[] lat;
	delete[] lon;
	delete[] idxTo;

	//Build and write vertex coordinate arrays
	x = new double[nVertices];
	y = new double[nVertices];
	z = new double[nVertices];
	lat = new double[nVertices];
	lon = new double[nVertices];
	idxTo = new int[nVertices];

	i = 0;
	for(pnt_itr = vertices.begin(); pnt_itr != vertices.end(); ++pnt_itr){
		x[i] = (*pnt_itr).x;
		y[i] = (*pnt_itr).y;
		z[i] = (*pnt_itr).z;
		if(!spherical){
			lat[i] = 0.0;
			lon[i] = 0.0;
		} else {
			lat[i] = (*pnt_itr).getLat();
			lon[i] = (*pnt_itr).getLon();
		}
		idxTo[i] = (*pnt_itr).idx+1;

		i++;
	}
	if (!(latVertexVar = grid.add_var("latVertex", ncDouble, nVerticesDim))) return NC_ERR;
	if (!latVertexVar->put(lat,nVertices)) return NC_ERR;
	if (!(lonVertexVar = grid.add_var("lonVertex", ncDouble, nVerticesDim))) return NC_ERR;
	if (!lonVertexVar->put(lon,nVertices)) return NC_ERR;
	if (!(xVertexVar = grid.add_var("xVertex", ncDouble, nVerticesDim))) return NC_ERR;
	if (!xVertexVar->put(x,nVertices)) return NC_ERR;
	if (!(yVertexVar = grid.add_var("yVertex", ncDouble, nVerticesDim))) return NC_ERR;
	if (!yVertexVar->put(y,nVertices)) return NC_ERR;
	if (!(zVertexVar = grid.add_var("zVertex", ncDouble, nVerticesDim))) return NC_ERR;
	if (!zVertexVar->put(z,nVertices)) return NC_ERR;
	if (!(idx2vertexVar = grid.add_var("indexToVertexID", ncInt, nVerticesDim))) return NC_ERR;
	if (!idx2vertexVar->put(idxTo, nVertices)) return NC_ERR;
	delete[] x;
	delete[] y;
	delete[] z;
	delete[] lat;
	delete[] lon;
	delete[] idxTo;

	grid.close();

	return 0;
}/*}}}*/
int outputCellConnectivity( const string outputFilename) {/*{{{*/
	/*****************************************************************
	 *
	 * This function writes all of the *OnCell arrays. Including
	 * cellsOnCell
	 * edgesOnCell
	 * verticesOnCell
	 * nEdgesonCell
	 *
	 * ***************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nCellsDim = grid.get_dim( "nCells" );
	NcDim *maxEdgesDim = grid.get_dim( "maxEdges" );

	int nCells = nCellsDim->size();
	int maxEdges = maxEdgesDim->size();
	int i, j;

	// define nc variables
	NcVar *cocVar, *nEocVar, *eocVar, *vocVar;

	int *tmp_arr;
	
	// Build and write COC array
	tmp_arr = new int[nCells*maxEdges];

	for(i = 0; i < nCells; i++){
		for(j = 0; j < maxEdges; j++){
			tmp_arr[i*maxEdges + j] = 0;
		}
	}

	i = 0;
	for(vec_int_itr = cellsOnCell.begin(); vec_int_itr != cellsOnCell.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*maxEdges + j] = (*int_itr) + 1;
			j++;
		}
		i++;
	}
	if (!(cocVar = grid.add_var("cellsOnCell", ncInt, nCellsDim, maxEdgesDim))) return NC_ERR;
	if (!cocVar->put(tmp_arr,nCells,maxEdges)) return NC_ERR;

	// Build and write EOC array
	for(i = 0; i < nCells; i++){
		for(j = 0; j < maxEdges; j++){
			tmp_arr[i*maxEdges + j] = 0;
		}
	}

	i = 0;
	for(vec_int_itr = edgesOnCell.begin(); vec_int_itr != edgesOnCell.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*maxEdges + j] = (*int_itr) + 1;	
			j++;
		}

		i++;
	}

	if (!(eocVar = grid.add_var("edgesOnCell", ncInt, nCellsDim, maxEdgesDim))) return NC_ERR;
	if (!eocVar->put(tmp_arr,nCells,maxEdges)) return NC_ERR;

	// Build and write VOC array 
	for(i = 0; i < nCells; i++){
		for(j = 0; j < maxEdges; j++){
			tmp_arr[i*maxEdges + j] = 0;
		}
	}

	i = 0;
	for(vec_int_itr = verticesOnCell.begin(); vec_int_itr != verticesOnCell.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*maxEdges + j] = (*int_itr) + 1;	
			j++;
		}
		i++;
	}

	if (!(vocVar = grid.add_var("verticesOnCell", ncInt, nCellsDim, maxEdgesDim))) return NC_ERR;
	if (!vocVar->put(tmp_arr,nCells,maxEdges)) return NC_ERR;
	delete[] tmp_arr;

	//Build and write nEOC array
	tmp_arr = new int[nCells];

	i = 0;
	for(vec_int_itr = edgesOnCell.begin(); vec_int_itr != edgesOnCell.end(); ++vec_int_itr){
		tmp_arr[i] = (*vec_int_itr).size();
		i++;
	}

	if (!(nEocVar = grid.add_var("nEdgesOnCell", ncInt, nCellsDim))) return NC_ERR;
	if (!nEocVar->put(tmp_arr,nCells)) return NC_ERR;
	verticesOnCell.clear();
	edgesOnCell.clear();

	delete[] tmp_arr;

	return 0;
}/*}}}*/
int outputEdgeConnectivity( const string outputFilename) {/*{{{*/
	/*****************************************************************
	 *
	 * This function writes all of the *OnEdge arrays. Including
	 * cellsOnEdge
	 * edgesOnEdge
	 * verticesOnEdge
	 * nEdgesOnEdge
	 *
	 * ***************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nEdgesDim = grid.get_dim( "nEdges" );
	NcDim *maxEdges2Dim = grid.get_dim( "maxEdges2" );
	NcDim *vertexDegreeDim = grid.get_dim( "vertexDegree" );
	NcDim *twoDim = grid.get_dim( "TWO" );

	// define nc variables
	NcVar *coeVar, *nEoeVar, *eoeVar, *voeVar;

	int nEdges = nEdgesDim->size();
	int maxEdges2 = maxEdges2Dim->size();
	int vertexDegree = vertexDegreeDim->size();
	int two = twoDim->size();
	int i, j;

	int *tmp_arr;

	// Build and write EOE array
	tmp_arr = new int[nEdges*maxEdges2];

	for(i = 0; i < nEdges; i++){
		for(j = 0; j < maxEdges2; j++){
			tmp_arr[i*maxEdges2 + j] = 0;
		}
	}

	i = 0;
	for(vec_int_itr = edgesOnEdge.begin(); vec_int_itr != edgesOnEdge.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*maxEdges2 + j] = (*int_itr) + 1;	
			j++;
		}

		i++;
	}

	if (!(eoeVar = grid.add_var("edgesOnEdge", ncInt, nEdgesDim, maxEdges2Dim))) return NC_ERR;
	if (!eoeVar->put(tmp_arr,nEdges,maxEdges2)) return NC_ERR;
	delete[] tmp_arr;

	// Build and write COE array
	tmp_arr = new int[nEdges*two];
	for(i = 0; i < nEdges; i++){
		for(j = 0; j < two; j++){
			tmp_arr[i*two + j] = 0;
		}
	}
	i = 0;
	for(vec_int_itr = cellsOnEdge.begin(); vec_int_itr != cellsOnEdge.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*two + j] = (*int_itr) + 1;	
			j++;
		}

		i++;
	}

	if (!(coeVar = grid.add_var("cellsOnEdge", ncInt, nEdgesDim, twoDim))) return NC_ERR;
	if (!coeVar->put(tmp_arr,nEdges,two)) return NC_ERR;

	// Build VOE array
	i = 0;
	for(vec_int_itr = verticesOnEdge.begin(); vec_int_itr != verticesOnEdge.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*two + j] = (*int_itr) + 1;	
			j++;
		}

		i++;
	}

	if (!(voeVar = grid.add_var("verticesOnEdge", ncInt, nEdgesDim, twoDim))) return NC_ERR;
	if (!voeVar->put(tmp_arr,nEdges,two)) return NC_ERR;
	delete[] tmp_arr;

	// Build and write nEoe array
	tmp_arr = new int[nEdges];
	i = 0;
	for(vec_int_itr = edgesOnEdge.begin(); vec_int_itr != edgesOnEdge.end(); ++vec_int_itr){
		tmp_arr[i] = (*vec_int_itr).size();
		i++;
	}

	if (!(nEoeVar = grid.add_var("nEdgesOnEdge", ncInt, nEdgesDim))) return NC_ERR;
	if (!nEoeVar->put(tmp_arr,nEdges)) return NC_ERR;

	delete[] tmp_arr;

	cellsOnEdge.clear();
//	verticesOnEdge.clear(); // Needed for Initial conditions.
	edgesOnEdge.clear();
	
	return 0;
}/*}}}*/
int outputVertexConnectivity( const string outputFilename) {/*{{{*/
	/*****************************************************************
	 *
	 * This function writes all of the *OnVertex arrays. Including
	 * cellsOnVertex
	 * edgesOnVertex
	 *
	 * ***************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nVerticesDim = grid.get_dim( "nVertices" );
	NcDim *vertexDegreeDim = grid.get_dim( "vertexDegree" );

	// define nc variables
	NcVar *covVar, *eovVar, *bdryVertVar;

	int nVertices = nVerticesDim->size();
	int vertexDegree = vertexDegreeDim->size();
	int i, j;

	int *tmp_arr;

	// Build and write COV array
	tmp_arr = new int[nVertices*vertexDegree];
	
	for(i = 0; i < nVertices; i++){
		for(j = 0; j < vertexDegree; j++){
			tmp_arr[i*vertexDegree + j] = 0;
		}
	}

	i = 0;
	for(vec_int_itr = cellsOnVertex.begin(); vec_int_itr != cellsOnVertex.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*vertexDegree + j] = (*int_itr) + 1;	
			j++;
		}
		i++;
	}

	if (!(covVar = grid.add_var("cellsOnVertex", ncInt, nVerticesDim, vertexDegreeDim))) return NC_ERR;
	if (!covVar->put(tmp_arr,nVertices,vertexDegree)) return NC_ERR;

	// Build and write EOV array
	for(i = 0; i < nVertices; i++){
		for(j = 0; j < vertexDegree; j++){
			tmp_arr[i*vertexDegree + j] = 0;
		}
	}
	i = 0;
	for(vec_int_itr = edgesOnVertex.begin(); vec_int_itr != edgesOnVertex.end(); ++vec_int_itr){
		j = 0;
		for(int_itr = (*vec_int_itr).begin(); int_itr != (*vec_int_itr).end(); ++int_itr){
			tmp_arr[i*vertexDegree + j] = (*int_itr) + 1;	
			j++;
		}

		i++;
	}
	if (!(eovVar = grid.add_var("edgesOnVertex", ncInt, nVerticesDim, vertexDegreeDim))) return NC_ERR;
	if (!eovVar->put(tmp_arr,nVertices,vertexDegree)) return NC_ERR;
	delete[] tmp_arr;

	// Build and write bdryVert array
	tmp_arr = new int[nVertices];
	
	i = 0;
	for(vec_int_itr = cellsOnVertex.begin(); vec_int_itr != cellsOnVertex.end(); ++vec_int_itr){
		if((*vec_int_itr).size() == vertexDegree){
			tmp_arr[i] = 0;
		} else {
			tmp_arr[i] = 1;
		}
		i++;
	}

	if (!(bdryVertVar = grid.add_var("boundaryVertex", ncInt, nVerticesDim))) return NC_ERR;
	if (!bdryVertVar->put(tmp_arr, nVertices)) return NC_ERR;

	delete[] tmp_arr;

	cellsOnVertex.clear();
	edgesOnVertex.clear();

	return 0;
}/*}}}*/
int outputCellParameters( const string outputFilename) {/*{{{*/
	/*********************************************************
	 *
	 * This function writes all cell parameters, including
	 * 	areaCell
	 *
	 * *******************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nCellsDim = grid.get_dim( "nCells" );

	// define nc variables
	NcVar *areacVar;

	int nCells = nCellsDim->size();
	int i, j;

	if (!(areacVar = grid.add_var("areaCell", ncDouble, nCellsDim))) return NC_ERR;
	if (!areacVar->put(&areaCell[0],nCells)) return NC_ERR;

	areaCell.clear();

	return 0;
}/*}}}*/
int outputVertexParameters( const string outputFilename) {/*{{{*/
	/*********************************************************
	 *
	 * This function writes all vertex parameters, including
	 * 	areaTriangle
	 * 	kiteAreasOnVertex
	 *
	 * *******************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nVerticesDim = grid.get_dim( "nVertices" );
	NcDim *vertexDegreeDim = grid.get_dim( "vertexDegree" );

	// define nc variables
	NcVar *areatVar;
	NcVar *kareaVar;

	int nVertices = nVerticesDim->size();
	int vertexDegree = vertexDegreeDim->size();
	int i, j;
	int count;

	double *tmp_arr;

	// Build and write areaTriangle
	if (!(areatVar = grid.add_var("areaTriangle", ncDouble, nVerticesDim))) return NC_ERR;
	if (!areatVar->put(&areaTriangle[0],nVertices)) return NC_ERR;

	// Build and write kiteAreasOnVertex
	// TODO: Fix kite area for quads?
	tmp_arr = new double[nVertices*vertexDegree];
	i = 0;
	count = 0;
	for(vec_dbl_itr = kiteAreasOnVertex.begin(); vec_dbl_itr != kiteAreasOnVertex.end(); ++vec_dbl_itr){
		j = 0;
		for(dbl_itr = (*vec_dbl_itr).begin(); dbl_itr != (*vec_dbl_itr).end(); ++dbl_itr){
			tmp_arr[i*vertexDegree + j] = (*dbl_itr);
			j++;
			count++;
		}
		i++;
	}

	if (!(kareaVar = grid.add_var("kiteAreasOnVertex", ncDouble, nVerticesDim, vertexDegreeDim))) return NC_ERR;
	if (!kareaVar->put(tmp_arr,nVertices,vertexDegree)) return NC_ERR;

	delete[] tmp_arr;

	kiteAreasOnVertex.clear();

	return 0;
}/*}}}*/
int outputEdgeParameters( const string outputFilename) {/*{{{*/
	/*********************************************************
	 *
	 * This function writes all grid parameters, including
	 *	angleEdge
	 *	dcEdge
	 *	dvEdge
	 *	weightsOnEdge
	 *
	 * *******************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;
	
	// fetch dimensions
	NcDim *nEdgesDim = grid.get_dim( "nEdges" );
	NcDim *maxEdges2Dim = grid.get_dim( "maxEdges2" );

	// define nc variables
	NcVar *angleVar;
	NcVar *kareaVar, *dcEdgeVar, *dvEdgeVar;
	NcVar *woeVar;

	int nEdges = nEdgesDim->size();
	int maxEdges2 = maxEdges2Dim->size();
	int i, j;

	double *tmp_arr;

	//Build and write angleEdge
	if (!(angleVar = grid.add_var("angleEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!angleVar->put(&angleEdge[0],nEdges)) return NC_ERR;

	//Build and write dcEdge
	if (!(dcEdgeVar = grid.add_var("dcEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!dcEdgeVar->put(&dcEdge[0],nEdges)) return NC_ERR;

	//Build and write dvEdge
	if (!(dvEdgeVar = grid.add_var("dvEdge", ncDouble, nEdgesDim))) return NC_ERR;
	if (!dvEdgeVar->put(&dvEdge[0],nEdges)) return NC_ERR;

	//Build and write weightsOnEdge
	tmp_arr = new double[nEdges*maxEdges2];

	i = 0;
	for(vec_dbl_itr = weightsOnEdge.begin(); vec_dbl_itr != weightsOnEdge.end(); ++vec_dbl_itr){
		for(j = 0; j < maxEdges2; j++){
			tmp_arr[i*maxEdges2 + j] = 0.0;
		}

		j = 0;
		for(dbl_itr = (*vec_dbl_itr).begin(); dbl_itr != (*vec_dbl_itr).end(); ++dbl_itr){
			tmp_arr[i*maxEdges2 + j] = (*dbl_itr);
			j++;
		}
		i++;
	}

	if (!(woeVar = grid.add_var("weightsOnEdge", ncDouble, nEdgesDim, maxEdges2Dim))) return NC_ERR;
	if (!woeVar->put(tmp_arr,nEdges,maxEdges2)) return NC_ERR;

	delete[] tmp_arr;

	angleEdge.clear();
	dcEdge.clear();
	dvEdge.clear();
	weightsOnEdge.clear();

	return 0;
}/*}}}*/
int outputMeshDensity( const string outputFilename) {/*{{{*/
	/***************************************************************************
	 *
	 * This function writes the meshDensity variable. Read in from the file SaveDensity
	 *
	 * *************************************************************************/
	// Return this code to the OS in case of failure.
	static const int NC_ERR = 2;
	
	// set error behaviour (matches fortran behaviour)
	NcError err(NcError::verbose_nonfatal);
	
	// open the scvtmesh file
	NcFile grid(outputFilename.c_str(), NcFile::Write);
	
	// check to see if the file was opened
	if(!grid.is_valid()) return NC_ERR;

	// fetch dimensions
	NcDim *nCellsDim = grid.get_dim( "nCells" );

	NcVar *cDensVar;

	int nCells = nCellsDim->size();
	int i, j, k;
	int junk_int;
	double junk_dbl;

	vector<double> dbl_tmp_arr;

	//Write meshDensity
	if (!(cDensVar = grid.add_var("meshDensity", ncDouble, nCellsDim))) return NC_ERR;
	if (!cDensVar->put(&meshDensity.at(0),nCells)) return NC_ERR;

	return 0;
}/*}}}*/
/*}}}*/

string gen_random(const int len) {/*{{{*/
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	string rand_str = "";

	for (int i = 0; i < len; ++i) {
		rand_str += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return rand_str;
}/*}}}*/
