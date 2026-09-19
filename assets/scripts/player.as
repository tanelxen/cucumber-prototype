class Player : MonoBehaviour
{
    float move_speed = 400.0f;
    float mouse_speed = 8.0f;

    float forwardmove = 0;
    float rightmove = 0;

    TransformComponent@ headTransform;

    void start()
    {
        print("Player spawned on entity: " + GetOwner().GetName());
    }

    void update(float dt)
    {
        TransformComponent@ transform = GetTransform();
        if (transform == null) return;

        forwardmove = 0;
        rightmove = 0;

        if (Input::GetKey(KeyCode::W))
        {
            forwardmove += 1;
        }
        
        if (Input::GetKey(KeyCode::S))
        {
            forwardmove -= 1;
        }
        
        if (Input::GetKey(KeyCode::D))
        {
            rightmove += 1;
        }
        
        if (Input::GetKey(KeyCode::A))
        {
            rightmove -= 1;
        }


        Vector3 forward = transform.GetForward();
        Vector3 right = transform.GetRight();

        Vector3 direction(0, 0, 0);
        direction += forward * forwardmove * move_speed;
        direction += right * rightmove * move_speed;
        transform.position += direction * dt;

        if (Input::GetRightMouseButton())
        {
            Input::SetCursorEnabled(false);

            float mouseX = Input::GetMouseOffsetX();
            transform.rotation.y -= mouseX * mouse_speed * dt;

            if (headTransform != null)
            {
                float mouseY = Input::GetMouseOffsetY();
                headTransform.rotation.x -= mouseY * mouse_speed * dt;
            }
        }
        else
        {
            Input::SetCursorEnabled(true);
        }
    }
}