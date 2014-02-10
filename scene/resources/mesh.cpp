/*************************************************************************/
/*  mesh.cpp                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#include "mesh.h"
#include "scene/resources/concave_polygon_shape.h"
#include "scene/resources/convex_polygon_shape.h"

static const char*_array_name[]={
	"vertex_array",
	"normal_array",
	"tangent_array",
	"color_array",
	"tex_uv_array",
	"tex_uv2_array",
	"bone_array",
	"weights_array",
	"index_array",
	NULL
};

static const Mesh::ArrayType _array_types[]={

	Mesh::ARRAY_VERTEX,
	Mesh::ARRAY_NORMAL,
	Mesh::ARRAY_TANGENT,
	Mesh::ARRAY_COLOR,
	Mesh::ARRAY_TEX_UV,
	Mesh::ARRAY_TEX_UV2,
	Mesh::ARRAY_BONES,
	Mesh::ARRAY_WEIGHTS,
	Mesh::ARRAY_INDEX
};


/* compatibility */
static const int _format_translate[]={

	Mesh::ARRAY_FORMAT_VERTEX,
	Mesh::ARRAY_FORMAT_NORMAL,
	Mesh::ARRAY_FORMAT_TANGENT,
	Mesh::ARRAY_FORMAT_COLOR,
	Mesh::ARRAY_FORMAT_TEX_UV,
	Mesh::ARRAY_FORMAT_TEX_UV2,
	Mesh::ARRAY_FORMAT_BONES,
	Mesh::ARRAY_FORMAT_WEIGHTS,
	Mesh::ARRAY_FORMAT_INDEX,
};


bool Mesh::_set(const StringName& p_name, const Variant& p_value) {

	String sname=p_name;

	if (p_name=="morph_target/names") {

		DVector<String> sk=p_value;
		int sz = sk.size();
		DVector<String>::Read r = sk.read();
		for(int i=0;i<sz;i++)
			add_morph_target(r[i]);
		return true;
	}

	if (p_name=="morph_target/mode") {

		set_morph_target_mode(MorphTargetMode(int(p_value)));
		return true;
	}

	if (sname.begins_with("materials/")) {

		int idx=sname.get_slice("/",1).to_int()-1;
		surface_set_material(idx,p_value);
		return true;
	}

	if (!sname.begins_with("surfaces"))
		return false;


	int idx=sname.get_slice("/",1).to_int();
	String what=sname.get_slice("/",2);

	if (idx==surfaces.size()) {

		if (what=="custom") {
			add_custom_surface(p_value);
			return true;

		}		

		//create
		Dictionary d=p_value;
		ERR_FAIL_COND_V(!d.has("primitive"),false);
		ERR_FAIL_COND_V(!d.has("arrays"),false);
		ERR_FAIL_COND_V(!d.has("morph_arrays"),false);

		bool alphasort = d.has("alphasort") && bool(d["alphasort"]);


		add_surface(PrimitiveType(int(d["primitive"])),d["arrays"],d["morph_arrays"],alphasort);
		if (d.has("material")) {

			surface_set_material(idx,d["material"]);
		}
		if (d.has("name")) {
			surface_set_name(idx,d["name"]);
		}


		return true;
	}

	if (what=="custom_aabb") {

		surface_set_custom_aabb(idx,p_value);
		return true;
	}

	return false;
}

bool Mesh::_get(const StringName& p_name,Variant &r_ret) const {

	String sname=p_name;

	if (p_name=="morph_target/names") {

		DVector<String> sk;
		for(int i=0;i<morph_targets.size();i++)
			sk.push_back(morph_targets[i]);
		r_ret=sk;
		return true;
	} else if (p_name=="morph_target/mode") {

		r_ret = get_morph_target_mode();
		return true;
	} else if (sname.begins_with("materials/")) {

		int idx=sname.get_slice("/",1).to_int()-1;
		r_ret=surface_get_material(idx);
		return true;

	} else if (!sname.begins_with("surfaces"))
		return false;


	int idx=sname.get_slice("/",1).to_int();
	ERR_FAIL_INDEX_V(idx,surfaces.size(),false);

	Dictionary d;
	d["primitive"]=surface_get_primitive_type(idx);
	d["arrays"]=surface_get_arrays(idx);
	d["morph_arrays"]=surface_get_morph_arrays(idx);
	d["alphasort"]=surface_is_alpha_sorting_enabled(idx);
	Ref<Material> m = surface_get_material(idx);
	if (m.is_valid())
		d["material"]=m;
	String n = surface_get_name(idx);
	if (n!="")
		d["name"]=n;

	r_ret=d;

	return true;
}

void Mesh::_get_property_list( List<PropertyInfo> *p_list) const {

	if (morph_targets.size()) {
		p_list->push_back(PropertyInfo(Variant::STRING_ARRAY,"morph_target/names",PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::INT,"morph_target/mode",PROPERTY_HINT_ENUM,"Normalized,Relative"));
	}

	for (int i=0;i<surfaces.size();i++) {
		
		p_list->push_back( PropertyInfo( Variant::DICTIONARY,"surfaces/"+itos(i), PROPERTY_HINT_NONE,"",PROPERTY_USAGE_NOEDITOR ) );
		p_list->push_back( PropertyInfo( Variant::OBJECT,"materials/"+itos(i+1), PROPERTY_HINT_RESOURCE_TYPE,"Material",PROPERTY_USAGE_EDITOR ) );
	}
}


void Mesh::_recompute_aabb() {

	// regenerate AABB
	aabb=AABB();
		
	for (int i=0;i<surfaces.size();i++) {
	
		if (i==0)
			aabb=surfaces[i].aabb;
		else
			aabb.merge_with(surfaces[i].aabb);
	}

}

void Mesh::add_surface(PrimitiveType p_primitive,const Array& p_arrays,const Array& p_blend_shapes,bool p_alphasort) {


	ERR_FAIL_COND(p_arrays.size()!=ARRAY_MAX);

	Surface s;

	VisualServer::get_singleton()->mesh_add_surface(mesh,(VisualServer::PrimitiveType)p_primitive, p_arrays,p_blend_shapes,p_alphasort);
	surfaces.push_back(s);



	/* make aABB? */ {

		DVector<Vector3> vertices=p_arrays[ARRAY_VERTEX];
		int len=vertices.size();
		ERR_FAIL_COND(len==0);
		DVector<Vector3>::Read r=vertices.read();
		const Vector3 *vtx=r.ptr();

		// check AABB
		AABB aabb;
		for (int i=0;i<len;i++) {

			if (i==0)
				aabb.pos=vtx[i];
			else
				aabb.expand_to(vtx[i]);
		}

		surfaces[surfaces.size()-1].aabb=aabb;
		surfaces[surfaces.size()-1].alphasort=p_alphasort;

		_recompute_aabb();

	}

	triangle_mesh=Ref<TriangleMesh>();
	_change_notify();

}

Array Mesh::surface_get_arrays(int p_surface) const {

	ERR_FAIL_INDEX_V(p_surface,surfaces.size(),Array());
	return VisualServer::get_singleton()->mesh_get_surface_arrays(mesh,p_surface);

}
Array Mesh::surface_get_morph_arrays(int p_surface) const {

	ERR_FAIL_INDEX_V(p_surface,surfaces.size(),Array());
	return VisualServer::get_singleton()->mesh_get_surface_morph_arrays(mesh,p_surface);

}



void Mesh::add_custom_surface(const Variant& p_data) {

	Surface s;
	s.aabb=AABB();
	VisualServer::get_singleton()->mesh_add_custom_surface(mesh,p_data);
	surfaces.push_back(s);

	triangle_mesh=Ref<TriangleMesh>();
	_change_notify();
}


int Mesh::get_surface_count() const {

	return surfaces.size();
}

void Mesh::add_morph_target(const StringName& p_name) {

	if (surfaces.size()) {
		ERR_EXPLAIN("Can't add a shape key count if surfaces are already created.");
		ERR_FAIL_COND(surfaces.size());
	}

	StringName name=p_name;

	if (morph_targets.find(name)!=-1 ) {

		int count=2;
		do {

			name = String(p_name) + " " + itos(count);
			count++;
		} while(morph_targets.find(name)!=-1);
	}

	morph_targets.push_back(name);
	VS::get_singleton()->mesh_set_morph_target_count(mesh,morph_targets.size());

}


int Mesh::get_morph_target_count() const {

	return morph_targets.size();
}
StringName Mesh::get_morph_target_name(int p_index) const {
	ERR_FAIL_INDEX_V( p_index, morph_targets.size(),StringName() );
	return morph_targets[p_index];
}
void Mesh::clear_morph_targets() {

	if (surfaces.size()) {
		ERR_EXPLAIN("Can't set shape key count if surfaces are already created.");
		ERR_FAIL_COND(surfaces.size());
	}

	morph_targets.clear();
}

void Mesh::set_morph_target_mode(MorphTargetMode p_mode) {

	morph_target_mode=p_mode;
	VS::get_singleton()->mesh_set_morph_target_mode(mesh,(VS::MorphTargetMode)p_mode);
}

Mesh::MorphTargetMode Mesh::get_morph_target_mode() const {

	return morph_target_mode;
}


void Mesh::surface_remove(int p_idx) {

	ERR_FAIL_INDEX(p_idx, surfaces.size() );
	VisualServer::get_singleton()->mesh_remove_surface(mesh,p_idx);
	surfaces.remove(p_idx);
	
	triangle_mesh=Ref<TriangleMesh>();
	_recompute_aabb();
	_change_notify();
}





int Mesh::surface_get_array_len(int p_idx) const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), -1 );
	return VisualServer::get_singleton()->mesh_surface_get_array_len( mesh, p_idx );

}

int Mesh::surface_get_array_index_len(int p_idx) const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), -1 );
	return VisualServer::get_singleton()->mesh_surface_get_array_index_len( mesh, p_idx );

}

uint32_t Mesh::surface_get_format(int p_idx) const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), 0 );
	return VisualServer::get_singleton()->mesh_surface_get_format( mesh, p_idx );

}



Mesh::PrimitiveType Mesh::surface_get_primitive_type(int p_idx) const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), PRIMITIVE_LINES );
	return (PrimitiveType)VisualServer::get_singleton()->mesh_surface_get_primitive_type( mesh, p_idx );
}

bool Mesh::surface_is_alpha_sorting_enabled(int p_idx) const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), 0 );
	return surfaces[p_idx].alphasort;
}

void Mesh::surface_set_material(int p_idx, const Ref<Material>& p_material) {

	ERR_FAIL_INDEX( p_idx, surfaces.size() );
	if (surfaces[p_idx].material==p_material)
		return;
	surfaces[p_idx].material=p_material;
	VisualServer::get_singleton()->mesh_surface_set_material(mesh, p_idx, p_material.is_null()?RID():p_material->get_rid());

	_change_notify("material");
}

void Mesh::surface_set_name(int p_idx, const String& p_name) {

	ERR_FAIL_INDEX( p_idx, surfaces.size() );

	surfaces[p_idx].name=p_name;
}

String Mesh::surface_get_name(int p_idx) const{

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(),String() );
	return surfaces[p_idx].name;

}

void Mesh::surface_set_custom_aabb(int p_idx,const AABB& p_aabb) {

	ERR_FAIL_INDEX( p_idx, surfaces.size() );
	surfaces[p_idx].aabb=p_aabb;
// set custom aabb too?
}

Ref<Material> Mesh::surface_get_material(int p_idx)  const {

	ERR_FAIL_INDEX_V( p_idx, surfaces.size(), Ref<Material>() );
	return surfaces[p_idx].material;

}

void Mesh::add_surface_from_mesh_data(const Geometry::MeshData& p_mesh_data) {

	VisualServer::get_singleton()->mesh_add_surface_from_mesh_data( mesh, p_mesh_data );
	AABB aabb;
	for (int i=0;i<p_mesh_data.vertices.size();i++) {
	
		if (i==0)
			aabb.pos=p_mesh_data.vertices[i];
		else
			aabb.expand_to(p_mesh_data.vertices[i]);
	}

	
	Surface s;
	s.aabb=aabb;
	if (surfaces.size()==0)
		aabb=s.aabb;
	else
		aabb.merge_with(s.aabb);

	triangle_mesh=Ref<TriangleMesh>();

	surfaces.push_back(s);
	_change_notify();
}

RID Mesh::get_rid() const {

	return mesh;
}
AABB Mesh::get_aabb() const {

	return aabb;
}

DVector<Face3> Mesh::get_faces() const {


	Ref<TriangleMesh> tm = generate_triangle_mesh();
	if (tm.is_valid())
		return tm->get_faces();
	return DVector<Face3>();
/*
	for (int i=0;i<surfaces.size();i++) {
	
		if (VisualServer::get_singleton()->mesh_surface_get_primitive_type( mesh, i ) != VisualServer::PRIMITIVE_TRIANGLES )
			continue;
	
		DVector<int> indices;
		DVector<Vector3> vertices;
		
		vertices=VisualServer::get_singleton()->mesh_surface_get_array(mesh, i,VisualServer::ARRAY_VERTEX);
		
		int len=VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, i);
		bool has_indices;
		
		if (len>0) {
		
			indices=VisualServer::get_singleton()->mesh_surface_get_array(mesh, i,VisualServer::ARRAY_INDEX);
			has_indices=true;
			
		} else {
		
			len=vertices.size();
			has_indices=false;
		}
	
		if (len<=0)
			continue;
			
		DVector<int>::Read indicesr = indices.read();
		const int *indicesptr = indicesr.ptr();
		
		DVector<Vector3>::Read verticesr = vertices.read();
		const Vector3 *verticesptr = verticesr.ptr();
		
		int old_faces=faces.size();
		int new_faces=old_faces+(len/3);
		
		faces.resize(new_faces);
		
		DVector<Face3>::Write facesw = faces.write();
		Face3 *facesptr=facesw.ptr();
		
	
		for (int i=0;i<len/3;i++) {
		
			Face3 face;
			
			for (int j=0;j<3;j++) {
			
				int idx=i*3+j;
				face.vertex[j] = has_indices ? verticesptr[ indicesptr[ idx ] ] : verticesptr[idx];
			}
			
			facesptr[i+old_faces]=face;
		}
		
	}
*/

}

Ref<Shape> Mesh::create_convex_shape() const {

	DVector<Vector3> vertices;

	for(int i=0;i<get_surface_count();i++) {

		Array a = surface_get_arrays(i);
		DVector<Vector3> v=a[ARRAY_VERTEX];
		vertices.append_array(v);

	}

	Ref<ConvexPolygonShape> shape = memnew( ConvexPolygonShape );
	shape->set_points(vertices);
	return shape;
}

Ref<Shape> Mesh::create_trimesh_shape() const {

	DVector<Face3> faces = get_faces();
	if (faces.size()==0)
		return Ref<Shape>();

	DVector<Vector3> face_points;
	face_points.resize( faces.size()*3 );

	for (int i=0;i<face_points.size();i++) {

		Face3 f = faces.get( i/3 );
		face_points.set(i, f.vertex[i%3] );
	}

	Ref<ConcavePolygonShape> shape = memnew( ConcavePolygonShape );
	shape->set_faces(face_points);
	return shape;
}

void Mesh::center_geometry() {

/*
	Vector3 ofs = aabb.pos+aabb.size*0.5;

	for(int i=0;i<get_surface_count();i++) {

		DVector<Vector3> geom = surface_get_array(i,ARRAY_VERTEX);
		int gc =geom.size();
		DVector<Vector3>::Write w = geom.write();
		surfaces[i].aabb.pos-=ofs;

		for(int i=0;i<gc;i++) {

			w[i]-=ofs;
		}

		w = DVector<Vector3>::Write();

		surface_set_array(i,ARRAY_VERTEX,geom);

	}

	aabb.pos-=ofs;

*/

}

Ref<TriangleMesh> Mesh::generate_triangle_mesh() const {

	if (triangle_mesh.is_valid())
		return triangle_mesh;

	int facecount=0;

	for(int i=0;i<get_surface_count();i++) {

		if (surface_get_primitive_type(i)!=PRIMITIVE_TRIANGLES)
			continue;

		if (surface_get_format(i)&ARRAY_FORMAT_INDEX) {

			facecount+=surface_get_array_index_len(i);
		} else {

			facecount+=surface_get_array_len(i);
		}

	}

	if (facecount==0 || (facecount%3)!=0)
		return triangle_mesh;

	DVector<Vector3> faces;
	faces.resize(facecount);
	DVector<Vector3>::Write facesw=faces.write();

	int widx=0;

	for(int i=0;i<get_surface_count();i++) {

		if (surface_get_primitive_type(i)!=PRIMITIVE_TRIANGLES)
			continue;

		Array a = surface_get_arrays(i);

		int vc = surface_get_array_len(i);
		DVector<Vector3> vertices = a[ARRAY_VERTEX];
		DVector<Vector3>::Read vr=vertices.read();

		if (surface_get_format(i)&ARRAY_FORMAT_INDEX) {

			int ic=surface_get_array_index_len(i);
			DVector<int> indices = a[ARRAY_INDEX];
			DVector<int>::Read ir = indices.read();

			for(int i=0;i<ic;i++)
				facesw[widx++]=vr[ ir[i] ];

		} else {

			for(int i=0;i<vc;i++)
				facesw[widx++]=vr[ i ];
		}

	}

	facesw=DVector<Vector3>::Write();


	triangle_mesh = Ref<TriangleMesh>( memnew( TriangleMesh ));
	triangle_mesh->create(faces);

	return triangle_mesh;


}

void Mesh::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_morph_target","name"),&Mesh::add_morph_target);
	ObjectTypeDB::bind_method(_MD("get_morph_target_count"),&Mesh::get_morph_target_count);
	ObjectTypeDB::bind_method(_MD("get_morph_target_name","index"),&Mesh::get_morph_target_name);
	ObjectTypeDB::bind_method(_MD("clear_morph_targets"),&Mesh::clear_morph_targets);
	ObjectTypeDB::bind_method(_MD("set_morph_target_mode","mode"),&Mesh::set_morph_target_mode);
	ObjectTypeDB::bind_method(_MD("get_morph_target_mode"),&Mesh::get_morph_target_mode);

	ObjectTypeDB::bind_method(_MD("add_surface","primitive","arrays","morph_arrays"),&Mesh::add_surface,DEFVAL(Array()));
	ObjectTypeDB::bind_method(_MD("get_surface_count"),&Mesh::get_surface_count);
	ObjectTypeDB::bind_method(_MD("surface_remove","surf_idx"),&Mesh::surface_remove);
	ObjectTypeDB::bind_method(_MD("surface_get_array_len","surf_idx"),&Mesh::surface_get_array_len);
	ObjectTypeDB::bind_method(_MD("surface_get_array_index_len","surf_idx"),&Mesh::surface_get_array_index_len);
	ObjectTypeDB::bind_method(_MD("surface_get_format","surf_idx"),&Mesh::surface_get_format);
	ObjectTypeDB::bind_method(_MD("surface_get_primitive_type","surf_idx"),&Mesh::surface_get_primitive_type);
	ObjectTypeDB::bind_method(_MD("surface_set_material","surf_idx","material:Material"),&Mesh::surface_set_material);
	ObjectTypeDB::bind_method(_MD("surface_get_material:Material","surf_idx"),&Mesh::surface_get_material);
	ObjectTypeDB::bind_method(_MD("surface_set_name","surf_idx","name"),&Mesh::surface_set_name);
	ObjectTypeDB::bind_method(_MD("surface_get_name","surf_idx"),&Mesh::surface_get_name);
	ObjectTypeDB::bind_method(_MD("center_geometry"),&Mesh::center_geometry);
	ObjectTypeDB::set_method_flags(get_type_static(),_SCS("center_geometry"),METHOD_FLAGS_DEFAULT|METHOD_FLAG_EDITOR);



	BIND_CONSTANT( NO_INDEX_ARRAY );
	BIND_CONSTANT( ARRAY_WEIGHTS_SIZE );
	
	BIND_CONSTANT( ARRAY_VERTEX );
	BIND_CONSTANT( ARRAY_NORMAL );
	BIND_CONSTANT( ARRAY_TANGENT );
	BIND_CONSTANT( ARRAY_COLOR );
	BIND_CONSTANT( ARRAY_TEX_UV );	
	BIND_CONSTANT( ARRAY_TEX_UV2 );
	BIND_CONSTANT( ARRAY_BONES );
	BIND_CONSTANT( ARRAY_WEIGHTS );
	BIND_CONSTANT( ARRAY_INDEX );
	
	BIND_CONSTANT( ARRAY_FORMAT_VERTEX );
	BIND_CONSTANT( ARRAY_FORMAT_NORMAL );
	BIND_CONSTANT( ARRAY_FORMAT_TANGENT );
	BIND_CONSTANT( ARRAY_FORMAT_COLOR );
	BIND_CONSTANT( ARRAY_FORMAT_TEX_UV );
	BIND_CONSTANT( ARRAY_FORMAT_TEX_UV2 );
	BIND_CONSTANT( ARRAY_FORMAT_BONES );
	BIND_CONSTANT( ARRAY_FORMAT_WEIGHTS );
	BIND_CONSTANT( ARRAY_FORMAT_INDEX );
		
	BIND_CONSTANT( PRIMITIVE_POINTS );
	BIND_CONSTANT( PRIMITIVE_LINES );
	BIND_CONSTANT( PRIMITIVE_LINE_STRIP );
	BIND_CONSTANT( PRIMITIVE_LINE_LOOP );
	BIND_CONSTANT( PRIMITIVE_TRIANGLES );
	BIND_CONSTANT( PRIMITIVE_TRIANGLE_STRIP );
	BIND_CONSTANT( PRIMITIVE_TRIANGLE_FAN );

}



Mesh::Mesh() {

	mesh=VisualServer::get_singleton()->mesh_create();
	morph_target_mode=MORPH_MODE_RELATIVE;

}


Mesh::~Mesh() {

	VisualServer::get_singleton()->free(mesh);
}


