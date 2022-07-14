// csx2dif.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "tinyxml2.h"
#include "glm\glm.hpp"
#include "glm\gtx\transform.hpp"
#include "glm\gtx\matrix_decompose.hpp"
#include "glm\gtx\quaternion.hpp"
#include <vector>
#include "DIFBuilder\DIFBuilder.hpp"

struct TexGen
{
	glm::vec4 axisU;
	glm::vec4 axisV;
	float rotation;
	float scale[2];
};

glm::mat4x4 parseMatrix(const char* str)
{
	float a, b, c, d;
	float e, f, g, h;
	float i, j, k, l;
	float m, n, o, p;
	sscanf(str, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", &a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p);
	return glm::mat4x4(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
}

glm::vec3 parseVec(const char* str)
{
	float x, y, z;
	sscanf(str, "%f %f %f", &x, &y, &z);
	return glm::vec3(x, y, z);
}

glm::vec2 parseTexSizes(const char* str)
{
	glm::vec2 b;
	sscanf(str, "%f %f", &b.x, &b.y);
	return b;
}

TexGen parseTexgenPlanes(const char* str)
{
	TexGen texgen;
	sscanf(str, "%f %f %f %f %f %f %f %f %f %f %f", &texgen.axisU.x, &texgen.axisU.y, &texgen.axisU.z, &texgen.axisU.w, &texgen.axisV.x, &texgen.axisV.y, &texgen.axisV.z, &texgen.axisV.w, &texgen.rotation, &texgen.scale[0], &texgen.scale[1]); //TODO: rotation and scaling values
	return texgen;
}

glm::vec2 computeUV(TexGen& texgen, glm::vec3 vertex, float* texsizes)
{
	if (texgen.scale[0] * texgen.scale[1] == 0)
		return glm::vec2(0, 0);

	glm::vec4 axisU = texgen.axisU;
	glm::vec4 axisV = texgen.axisV;

	if (fmodf(texgen.rotation, 360) != 0)
	{
		glm::vec3 updir = glm::cross(glm::vec3(axisU), glm::vec3(axisV));
		glm::mat4 rotMat = glm::rotate((float)(texgen.rotation * 3.14159265 / 180), updir);
		axisU = rotMat * axisU;
		axisV = rotMat * axisV;
	}

	glm::vec2 target = glm::vec2(glm::dot(glm::vec3(axisU), vertex), glm::dot(glm::vec3(axisV), vertex));
	target[0] *= (1 / texgen.scale[0]) * (32 / texsizes[0]);
	target[1] *= (-1 / texgen.scale[1]) * (32 / texsizes[1]);

	target[0] += texgen.axisU.w / texsizes[0];
	target[1] -= texgen.axisV.w / texsizes[1];

	return target;
}

std::vector<int> parseIndices(const char* str)
{
	std::vector<int> tempIndices;

	char* strs = (char*)malloc(strlen(str) + 1);
	strcpy(strs, str);

	char* tokn = strtok(strs, " ");

	while (tokn != NULL)
	{
		tempIndices.push_back(atoi(tokn));
		tokn = strtok(NULL," ");
	}
	
	std::vector<int> triangulatedIndices;
	
	for (int i = 0; i < tempIndices.size() - 2; i++)
	{
		triangulatedIndices.push_back(tempIndices[0]);
		triangulatedIndices.push_back(tempIndices[i + 1]);
		triangulatedIndices.push_back(tempIndices[i + 2]);
	}

	free(strs);

	return triangulatedIndices;
}

int main(int argc, const char** argv)
{
	printf("csx2dif 1.0\n");

	tinyxml2::XMLDocument csx;

	FILE *f;
	f = fopen(argv[1], "rb");

	DIF::DIFBuilder builder;

	std::map<int, DIF::DIFBuilder*> mpbuilders;

	//We load the csx file using the xml parser
	printf("Loading CSX\n");
	int loadcode = csx.LoadFile(f);
	if (loadcode == tinyxml2::XML_SUCCESS)
	{
		//Navigate our way to the xml node we want
		printf("Parsing CSX\n");
		tinyxml2::XMLElement* cscene = csx.FirstChildElement("ConstructorScene");
		tinyxml2::XMLElement* detail = cscene->FirstChildElement("DetailLevels")->FirstChildElement("DetailLevel");
		tinyxml2::XMLElement* interiormap = detail->FirstChildElement("InteriorMap");

		//Parse entities
		tinyxml2::XMLElement* entities = interiormap->FirstChildElement("Entities");
		tinyxml2::XMLElement* entity = entities->FirstChildElement();

		//This is for looking up what the entity is from its id, used for MPs and path_nodes(constructor doesnt do pathnodes like this but I added it as a new feature)
		std::map<int, tinyxml2::XMLElement*> grouplookup;


		//Parse Brushes
		tinyxml2::XMLElement* brushes = interiormap->FirstChildElement("Brushes");
		tinyxml2::XMLElement* brush = brushes->FirstChildElement();

		glm::vec3 min(1e8, 1e8, 1e8), max(-1e8, -1e8, -1e8);

		while (brush != NULL)
		{
			const char* transformattrib = brush->Attribute("transform");

			glm::mat4x4 transform = parseMatrix(transformattrib);

			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;

			glm::decompose(transform, scale, rotation, translation, skew, perspective);

			glm::mat4 finaltransform = glm::toMat4(rotation);
			finaltransform[0] *= scale.x;
			finaltransform[1] *= scale.y;
			finaltransform[2] *= scale.z;
			finaltransform[0][3] = translation.x;
			finaltransform[2][3] = translation.y;
			finaltransform[2][3] = translation.z;

			glm::vec3 off(transform[0][3], transform[1][3], transform[2][3]);

			tinyxml2::XMLElement* verticesElement = brush->FirstChildElement("Vertices");
			tinyxml2::XMLElement* vertex = verticesElement->FirstChildElement();

			while (vertex != NULL)
			{
				glm::vec3 vertexvec = parseVec(vertex->Attribute("pos"));

				glm::vec3 transformed = glm::vec3(finaltransform * glm::vec4(vertexvec, 1)) + off;

				if (min.x > transformed.x)
					min.x = transformed.x;
				if (min.y > transformed.y)
					min.y = transformed.y;
				if (min.z > transformed.z)
					min.z = transformed.z;

				if (max.x < transformed.x)
					max.x = transformed.x;
				if (max.y < transformed.y)
					max.y = transformed.y;
				if (max.z < transformed.z)
					max.z = transformed.z;


				vertex = vertex->NextSiblingElement();
			}

			brush = brush->NextSiblingElement();
		}

		glm::vec3 totaloff = (max - min) * 0.5f + glm::vec3(50, 50, 50);

		brushes = interiormap->FirstChildElement("Brushes");
		brush = brushes->FirstChildElement();

		//Iterate over all entities
		while (entity != NULL)
		{
			const char* classname = entity->Attribute("classname");
			if (strcmp(classname, "worldspawn") == 0) //We arent exporting this
			{
				entity = entity->NextSiblingElement();
				continue;
			}
			if (strcmp(classname, "Door_Elevator") == 0 || strcmp(classname, "path_node") == 0) 
			{
				//We put this entity in the group lookup, would be needed later on
				grouplookup.insert(std::pair<int, tinyxml2::XMLElement*>(atoi(entity->Attribute("id")), entity));
				entity = entity->NextSiblingElement();
				continue;
			}

			//Parse the GameEntities
			DIF::GameEntity ge;
			ge.position = parseVec(entity->Attribute("origin")) + totaloff;
			ge.datablock = classname;
			
			tinyxml2::XMLElement* properties = entity->FirstChildElement("Properties");
			const tinyxml2::XMLAttribute* prop = properties->FirstAttribute();

			while (prop != NULL)
			{
				ge.properties.push_back(std::pair<std::string, std::string>(std::string(prop->Name()), std::string(prop->Value())));
				prop = prop->Next();
			}

			ge.gameClass = std::string(properties->Attribute("game_class"));

			builder.addEntity(ge);

			entity = entity->NextSiblingElement();
				
		}

		while (brush != NULL)
		{

			int owner = brush->IntAttribute("owner");
			int type = brush->IntAttribute("type");

			bool isMP = false;
			if (owner != 0 && type == 999)
				isMP = true;

			const char* transformattrib = brush->Attribute("transform");

			glm::mat4x4 transform = parseMatrix(transformattrib);

			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;

			glm::decompose(transform, scale, rotation, translation, skew, perspective);

			translation = glm::vec3(transform[0][3], transform[1][3], transform[2][3]);

			glm::mat4 finaltransform = glm::toMat4(rotation);
			finaltransform[0] *= scale.x;
			finaltransform[1] *= scale.y;
			finaltransform[2] *= scale.z;
			finaltransform[0][3] = translation.x;
			finaltransform[1][3] = translation.y;
			finaltransform[2][3] = translation.z;


			std::vector<glm::vec3> vertices;
			std::vector<glm::vec3> transformedVertices;

			tinyxml2::XMLElement* verticesElement = brush->FirstChildElement("Vertices");
			tinyxml2::XMLElement* vertex = verticesElement->FirstChildElement();

			while (vertex != NULL)
			{
				glm::vec3 vertexvec = parseVec(vertex->Attribute("pos"));

				vertices.push_back(vertexvec);
				transformedVertices.push_back(glm::vec3(finaltransform * glm::vec4(vertexvec, 1)) + translation);
				vertex = vertex->NextSiblingElement();
			}

			tinyxml2::XMLElement* face = brush->FirstChildElement("Face");

			while (face != NULL)
			{
				//parseIndices just triangle-fans all the indices internally
				std::vector<int> indices = parseIndices(face->FirstChildElement("Indices")->Attribute("indices"));

				glm::vec2 texSizevec = parseTexSizes(face->Attribute("texDiv"));
				float texsizes[2];
				texsizes[0] = texSizevec.x;
				texsizes[1] = texSizevec.y;

				TexGen texgenplanes = parseTexgenPlanes(face->Attribute("texgens"));

				for (int i = 0; i < indices.size(); i+=3)
				{
					glm::vec3 p1 = vertices[indices[i]];
					glm::vec3 p2 = vertices[indices[i+1]];
					glm::vec3 p3 = vertices[indices[i+2]];

					glm::vec2 uv1 = computeUV(texgenplanes, p1, texsizes);
					glm::vec2 uv2 = computeUV(texgenplanes, p2, texsizes); 
					glm::vec2 uv3 = computeUV(texgenplanes, p3, texsizes); 

					DIF::DIFBuilder::Point pt1 = DIF::DIFBuilder::Point(transformedVertices[indices[i]] + totaloff, uv1);
					DIF::DIFBuilder::Point pt2 = DIF::DIFBuilder::Point(transformedVertices[indices[i+1]] + totaloff, uv2);
					DIF::DIFBuilder::Point pt3 = DIF::DIFBuilder::Point(transformedVertices[indices[i+2]] + totaloff, uv3);

					DIF::DIFBuilder::Triangle tri;

					tri.points[0].vertex = transformedVertices[indices[i]] + totaloff;
					tri.points[1].vertex = transformedVertices[indices[i+1]] + totaloff;
					tri.points[2].vertex = transformedVertices[indices[i+2]] + totaloff;

					tri.points[0].uv = uv1;
					tri.points[1].uv = uv2;
					tri.points[2].uv = uv3;

					glm::vec3 norm = glm::normalize(glm::cross(transformedVertices[indices[i]] - transformedVertices[indices[i + 1]], transformedVertices[indices[i + 2]] - transformedVertices[indices[i + 1]]));

					tri.points[0].normal = norm;
					tri.points[1].normal = norm;
					tri.points[2].normal = norm;

					//We need to build separate interiors too if there are any MPs
					if (isMP)
					{
						auto mpbuilderptr = mpbuilders.find(owner);
						if (mpbuilderptr == mpbuilders.end())
						{
							DIF::DIFBuilder *db = new DIF::DIFBuilder();
							mpbuilders.insert(std::pair<int, DIF::DIFBuilder*>(owner, db));
							db->addTriangle(tri, face->Attribute("material"));
						}
						else
							(*mpbuilderptr).second->addTriangle(tri, face->Attribute("material"));
					}
					else
						builder.addTriangle(tri, face->Attribute("material"));
				}

				face = face->NextSiblingElement("Face");
			}

			brush = brush->NextSiblingElement();
		}

		printf("Building pathedInteriors if any exist\n");
		//Now build MPs if there are any
		for (auto& mpbuilder : mpbuilders)
		{
			DIF::DIF d;
			mpbuilder.second->build(d);

			tinyxml2::XMLElement* entity = grouplookup[mpbuilder.first]; //mpbuilder.first is actually the index of the Door_Elevator entity
			std::vector<tinyxml2::XMLElement*> pathnodes;
			for (auto& entity2 : grouplookup) //We collect all the pathnodes which are for this MP, new feature.
				if (entity2.second->IntAttribute("owner") == mpbuilder.first)
					pathnodes.push_back(entity2.second);

			if (pathnodes.size() == 0) //Well, we cant find any, so just put all of MPs like constructor does
			{
				for (auto& entity2 : grouplookup)
				{
					if (strcmp(entity2.second->Attribute("classname"), "path_node") == 0)
						pathnodes.push_back(entity2.second);
				}
			}
			//Parse pathnodes
			std::vector<DIF::DIFBuilder::Marker> markers;
			for (auto& pathnode : pathnodes)
			{
				DIF::DIFBuilder::Marker m;
				m.initialTargetPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialTargetPosition", -1);
				m.initialPathPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialPathPosition", 0);
				m.msToNext = pathnode->FirstChildElement("Properties")->IntAttribute("next_time");
				m.position = parseVec(pathnode->Attribute("origin"));
				m.smoothing = pathnode->FirstChildElement("Properties")->IntAttribute("smoothing");
				
				markers.push_back(m);
			}

			if (pathnodes.size() == 0) // Well rip no pathnodes!
			{
				DIF::DIFBuilder::Marker m;
				m.initialTargetPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialTargetPosition", -1);
				m.initialPathPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialPathPosition", 0);
				m.msToNext = 5000;
				m.position = glm::vec3(0);
				m.smoothing = 0;

				markers.push_back(m);
			}

			builder.addPathedInterior(d.interior[0], markers);
			
			delete mpbuilder.second;
		}

		//Finally build the DIF and write it
		printf("Starting conversion\n");
		DIF::DIF finaldif;
		builder.build(finaldif);
		
		std::string fname = std::string(argv[1]);
		fname = fname.substr(0, fname.length() - 3) + "dif";

		printf("Writing DIF file\n");
		std::ofstream outStr;
		outStr.open(std::string(argv[1]).substr(0, strlen(argv[1]) - 3) + "dif", std::ios::out | std::ios::binary);
		finaldif.write(outStr, DIF::Version());
	}
	else
	{
		printf("%s", csx.ErrorIDToName((tinyxml2::XMLError)loadcode));
	}
}
