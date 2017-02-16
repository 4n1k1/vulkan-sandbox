#include "math3d.h"
#include <math.h>

Matrix_4x4 get_perspective_projection_matrix(float vertical_fov, float aspect, float z_near, float z_far)
{
	Matrix_4x4 result;

	const float t = tanf(vertical_fov / 2.0f);

	result.data[ 0] = 1.0f / (aspect * t);
	result.data[ 5] = 1.0f / (t);
	result.data[10] = (z_far + z_near) / (z_far - z_near);
	result.data[11] = 1.0f;
	result.data[14] = -(2.0f * z_far * z_near) / (z_far - z_near);

	return result;
}

Matrix_4x4 get_view_matrix(Vector_3 eye, Vector_3 look_at, Vector_3 up)
{
	Matrix_4x4 result;

	Vector_3 f = get_normalized_vector(get_subtracted_vector(look_at, eye));
	Vector_3 s = get_normalized_vector(get_cross_product(up, f));
	Vector_3 u = get_cross_product(f, s);

	result.data[ 0] = s.x;
	result.data[ 4] = s.y;
	result.data[ 8] = s.z;

	result.data[ 1] = u.x;
	result.data[ 5] = u.y;
	result.data[ 9] = u.z;

	result.data[ 2] = f.x;
	result.data[ 6] = f.y;
	result.data[10] = f.z;

	result.data[12] = -get_dot_product(s, eye);
	result.data[13] = -get_dot_product(u, eye);
	result.data[14] = -get_dot_product(f, eye);

	return result;
}

Vector_3 get_subtracted_vector(const Vector_3 v0, const Vector_3 v1)
{
	Vector_3 result;

	result.x = v0.x - v1.x;
	result.y = v0.y - v1.y;
	result.z = v0.z - v1.z;

	return result;
}

Vector_3 get_normalized_vector(const Vector_3 original_vector)
{
	Vector_3 result;

	float original_vector_length = sqrtf(
		original_vector.x * original_vector.x +
		original_vector.y * original_vector.y +
		original_vector.z * original_vector.z
	);

	result.x = original_vector.x / original_vector_length;
	result.y = original_vector.y / original_vector_length;
	result.z = original_vector.z / original_vector_length;

	return result;
}

Vector_3 get_cross_product(const Vector_3 v0, const Vector_3 v1)
{
	Vector_3 result;

	result.x = v0.y * v1.z - v1.y * v0.z;
	result.y = v0.z * v1.x - v1.z * v0.x;
	result.z = v0.x * v1.y - v1.x * v0.y;

	return result;
}

float get_dot_product(const Vector_3 v0, const Vector_3 v1)
{
	float result = v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;

	return result;
}