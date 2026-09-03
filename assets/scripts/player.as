class Player : MonoBehaviour
{
    float speed = 6.0f;

    Weapon@ weapon;

    void start()
    {
        print("Player spawned on entity: " + GetOwner().GetName());
    }

    void update(float dt)
    {
        TransformComponent@ transform = GetTransform();
        if (transform == null) return;

        if (Input::GetKey(KeyCode::D))
        {
            transform.position.x += speed * dt;
        }

        if (Input::GetKey(KeyCode::A))
        {
            transform.position.x -= speed * dt;
        }

        if (Input::GetKey(KeyCode::W))
        {
            transform.position.z -= speed * dt;
        }

        if (Input::GetKey(KeyCode::S))
        {
            transform.position.z += speed * dt;
        }

        if (Input::GetKeyDown(KeyCode::Space))
        {
            if (weapon != null) {
                weapon.Fire();
            }
        }
    }
}