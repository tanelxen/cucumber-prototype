class Player : MonoBehaviour
{
    float move_speed = 400.0f;
    float mouse_speed = 8.0f;

    private float forwardmove = 0;
    private float rightmove = 0;
    private bool jump = false;

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
        jump = Input::GetKey(KeyCode::Space); // Добавили прыжок

        if (Input::GetKey(KeyCode::W)) forwardmove += 1;
        if (Input::GetKey(KeyCode::S)) forwardmove -= 1;
        if (Input::GetKey(KeyCode::D)) rightmove += 1;
        if (Input::GetKey(KeyCode::A)) rightmove -= 1;

        MovementComponent@ motor = GetOwner().GetMovement();

        if (motor != null)
        {
            motor.SetInput(forwardmove, rightmove, jump);
        }

        if (Input::GetRightMouseButton())
        {
            Input::SetCursorEnabled(false);

            float mouseX = Input::GetMouseOffsetX();
            transform.SetRotation(transform.GetRotation() - Vector3(0, mouseX * mouse_speed * dt, 0));

            if (headTransform != null)
            {
                float mouseY = Input::GetMouseOffsetY();
                headTransform.SetRotation(headTransform.GetRotation() - Vector3(mouseY * mouse_speed * dt, 0, 0));
            }
        }
        else
        {
            Input::SetCursorEnabled(true);
        }
    }
}